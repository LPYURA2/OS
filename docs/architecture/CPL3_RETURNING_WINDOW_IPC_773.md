# StyleOS 773 — returning CPL3 window IPC (isolated x86-64 hardware profile)

Base: accepted 772 `d9d6f3c2f0d869f74d4499351f8757ddda196a5b`.
Scope: **only** `--test-user-ipc` / `--test-user-ipc-deny` x86-64 guest profiles; no production Desktop syscall enabled. Previous test images remain separately built.

## Protocol and implementation

- A bounded, explicit 16-byte wire request carries version 1, GET_KEY opcode,
  window ID and reply size. Neither PID, session, CR3, physical address nor
  kernel pointer is accepted as user-provided authority.
- The user A program is an image-owned 4-KiB CPL3 executable page. It places
  a request, a 16-byte reply area and its original cookie within its private
  RX-disabled/RW-enabled stack page. It makes an actual `INT 0x80` with RAX=4,
  RDX=request VA and RCX=reply VA.
- On TSS entry stack, the privileged handler authenticates CS/SS, frame
  bounds, actual CR3, selected running slot and expected image-owned root.
  It preflights **both** virtual buffers within A's single private stack page,
  rejects overlapping buffers and independently walks the active owned PTEs
  to verify correct image-owned physical backing, U/S, RW and NX. It copies
  the request through trusted kernel-mapped backing, not by dereferencing
  the raw user pointer.
- Only after all checks does a kernel-side broker use the true scheduler PID,
  scheduler handle, verified root, active session and preclaimed window to
  consume one `native_window_channel` key event. It writes a 16-byte staged
  reply into the verified writable stack backing page. The handler sets the
  saved hardware RAX and the assembly path restores all GPRs and `IRETQ`s
  into the **same** CPL3 context. User code verifies return status and both
  reply words before issuing its ordinary YIELD to the trusted dispatcher.
- The negative guest first requests output to B's private data VA (which is
  not mapped into A). The broker must not consume the event, no output bytes
  change, and user code sees error 2. It then retries with a valid A stack
  destination. Both guests continue with B YIELD, A genuine foreign-page
  FAULT, and B EXIT. Only A's window IPC port is reaped on fault; the other
  process remains alive until its controlled exit.

## Limits and security

- This is a **single-CPU, fixed-image, non-reclaimable memory experiment**.
  There is no generic page-pinning/PMM, system-wide ISR-driven preemption,
  stable user syscall ABI, genuine authentication or production `.tea` syscall.
- The UI window here is a **system model**, not a rendered desktop window.
  Actual keyboard driver, Desktop compositor integration, package installation
  and true multi-user login remain later stages. A successful probe is not
  evidence that normal `.tea` apps can draw or receive input.
- `INT 0x80` is a dedicated diagnostic gate, not the production ABI.
  Everything new is behind the test flag; the ordinary Desktop only retains
  771's empty window-channel service.
- The broker uses only one prearmed key event and one reply. User pointers
  crossing two pages, large buffers, untrusted roots, foreign destinations,
  invalid protocol versions/operations and wrong window IDs are refused.

## Validation

```sh
python3 tools/test_user_ipc_773.py --output build/user-ipc-773-host
python3 tools/build.py --arch x86_64 --test-user-ipc --output build/user-ipc-773-good
python3 tools/build.py --arch x86_64 --test-user-ipc-deny --output build/user-ipc-773-deny
python3 tools/test_user_ipc_773_guest.py --output build/user-ipc-773-qemu
python3 tools/test.py --host --output build/host-regression
```

The real-QEMU validator requires both guest profiles, UART kernel frames,
private stack PTE evidence, real PID/CR3 and terminal cleanup; it does not
accept synthetic traces. CI results are authoritative only when the workflow
run for the exact branch commit finishes with success.

## Next 774

Design a production-capable syscall boundary and live, owned process memory
lifetime, then connect disk-loaded `.tea` programs to normal Desktop window
surfaces and input without confusing UI sessions with kernel PIDs. After real
app isolation, build authenticated account/login flow from the approved mockup,
then harden StyleFS user homes/ACL/integrity and finalize Explorer. Preserve
`Быстрый доступ` and the current richer Desktop.
