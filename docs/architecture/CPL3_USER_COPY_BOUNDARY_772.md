# StyleOS 772 — controlled INT 0x80 user→kernel copy proof

Base: accepted 771 `96834d3b48d81f577b3dce9af4d8d2286962b74f`.
New branch: `feature/styleos-cpl3-user-copy-772`.

## Scope and mechanism

This stage introduces **a dedicated x86-64 diagnostic profile**, not yet the
production syscall ABI, pixel compositor, or `.tea` in-desktop launcher.
Only the dedicated `--test-user-syscall` and `--test-user-syscall-deny`
profiles change the CPL3 program A and the kernel INT 0x80 handler. Normal
Desktop, 768–770 guest `.tea` and all older ARM64 profiles remain unchanged.

The CPL3 user program A stores its cookie at the end of its own private stack.
It performs a real `int $0x80` with `RAX=3`, `RDX` holding the user virtual
pointer and `RCX=8`. The trap path:

1. Obtains the caller **from the saved hardware frame and actual current CR3**;
   checks CPL3 CS/SS, TSS entry stack, IF disabled, selected root matching the
   currently scheduled user context. **Never trusts a user-supplied PID or root.**
2. Runs strict range checks on `RDX` without dereferencing it. The request must
   be entirely within the one admitted user stack page and be at most 32 bytes.
3. Walks the actual live image-owned four-level x86 page tables, independently
   confirming U/S, RW and NX flags and the exact reserved image-owned physical
   backing page. The root and table buffers were admitted and protected before
   the CPU entered the separate user root.
4. Copies **from a trusted kernel-accessible backing BSS page**, never from the
   raw untrusted virtual pointer, into a bounded kernel-local staging buffer.
5. Captures the observed 8-byte cookie and returns to the trusted cooperative
   dispatcher under SAFE CR3 before reading or printing the evidence.
6. Performs the old continuation: B user YIELD, A genuine user #PF on B's page,
   B EXIT; kernel reaps the context and process PID.

The negative mode makes CPL3 pass `0x12010000` (B's private data VA) as its
source pointer. The trap verifies that it cannot be in A's admitted user stack
page, denies it **before accessing backing bytes**, and runs the same accepted
four-event CPL3 lifecycle. Host tests also reject a mismatched root, zero,
oversized, wrapping, cross-page and foreign addresses, with outputs unchanged
on refusals. These two hardware profiles are mandatory CI checks; no synthetic
UART or skipping absent OVMF is allowed.

## Security boundaries / not proven

- This is a **copy-only, yielding INT80 trap**; it is *not* a production
  returning syscall with a stable ABI, kernel↔user copy_to_user, IPC dispatch,
  asynchronous IRQ, SMP or transparent syscall resume.
- The syscall does **not** yet call `nwc_publish_tile`/`nwc_receive` in ordinary
  Desktop; no rendered independent `.tea` surfaces or actual keyboard driver.
- The test's program A is an embedded bounded **assembly probe**, not an ELF
  read from `.tea`. The 769–770 `.tea` proof stays separate and unchanged.
- An authenticated user account, user filesystem ACL, real session login and
  a general physical memory allocator are still not implemented.
- Kernel memory is copied from known reserved image BSS after live PTE
  validation. This is not a general-purpose VA→PA translator; pages must not
  become mutable or be reclaimed concurrently until locking and lifetime
  management are implemented.

## Tests

```sh
python3 tools/test_user_copy_772.py --output build/772-user-copy-host
python3 tools/build.py --arch x86_64 --desktop-v0 --output build/772-desktop
python3 tools/test_user_syscall_772_guest.py --output build/772-user-syscall-qemu
```

The guest tester independently parses UART saved full user registers, process
roots, PTEs and the positive/negative operation; it requires two real QEMU
boot executions, full fault isolation trace, exact copied/denied source, and
one terminal halt. CI also keeps all previous 709–771 tests.

## Next: 773

Add a stable, versioned user request structure and `copy_to_user` under
hardware-authorized root and lifespan checks. Bind the copied request to a
**real** `native_window_channel` port only in a dedicated guest profile,
prove session/owner denial and termination cleanup under QEMU. Move the
verified ABI to Desktop only after an independently owned Desktop kernel
address-space and process lifecycle can support userspace callers safely.
Then implement actual `.tea` windows, login mockup, StyleFS ACL/user homes and
final Explorer by the approved mockups.
