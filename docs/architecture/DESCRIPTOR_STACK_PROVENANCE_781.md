# StyleOS 781 — observed x86 descriptor gates and active boot stack

## Scope

**Read-only diagnostic, not root admission.** 781 extends the independent
`--test-frame-grants` hardware experiment after 779/780 have restored and
verified the inherited UEFI `CR3`. It never writes GDT/IDT/TSS, never loads TR,
never switches the experimental SAFE/User roots, never enables IRQs and never
reinterprets an MMIO address as physical RAM. Ordinary Desktop remains
unchanged.

## Evidence and provenance

- Actual SGDT/SIDT and CS came from `arch_paging_prepare`, and the existing
  `descriptors_ok` validated the entire 256-gate IDT before the old CR3
  roundtrip. After restoration, 779 walks and validates both IDT pages (an
  unaligned 4096-byte range crossing two pages), all GDT pages and the live
  boot stack using the old CR3, before the 781 audit reads IDT bytes.
- `descriptor_state_781` checks the complete 4096-byte IDT, the 256 present
  interrupt gates, CS selectors, gate flags, upper reserved words and IST
  indices. TR must be a GDT-aligned selector in-bounds and cannot overlap
  the current CS descriptor. `TR=0` is recorded as **no installed TSS**;
  in that case a nonzero IST gate is refused.
- On a nonzero TR the analyzer can check numerical GDT bounds, but it **does
  not dereference** the TSS descriptor, does **not** validate its TSS base,
  does **not** read RSP0/IST pointers and makes no full-stack assertion.
- Current x86 RSP is sampled after the CR3 restoration and checked against
  the actual UEFI boot stack range. This verifies **one currently executing
  stack**, not all stacks, all CPUs, nested exceptions or interrupt stacks.
- `STYLEOS DSI 781 INHERITED` reports the original CR3, CS, TR, GDT/IDT
  limits, total gates, IST-gate count and mask, actual RSP, and `active=0`.
  Hardware verifier cross-checks it against KMI 779, KDI 780, the UEFI
  `MEMMAP STACK` range and the prior GDT/IDT UART records. Tampered `active`,
  IST or stack-bound assertions must fail.

## Security boundary

The entire report returns `activation_permitted=false`; this is deliberate.
A complete set of gates and a live boot RSP do **not** establish ownership
or safe mapping of future per-process TSS, IST stack pages, runtime services,
GOP framebuffer or MMIO, and do not establish section-level W^X. The
previous `KDI` blockers remain in effect. Do not activate detached CR3 roots
or reconfigure task registers based on this data.

## Tests

- `tools/test_descriptor_state_781.py` — ASAN/UBSAN negative tests, explicit
  failure for malformed selectors/gates, byte bounds and read-only COFF compile.
- `tools/test_frame_grant_guest.py` — **real QEMU/OVMF**, independent UART
  provenance checks and tamper-sensitive negative cases, in addition to all
  inherited frame-grant, CR3 restoration, KMI 779 and KDI 780 assertions.
- Existing host, Desktop, `.tea`, CPL3 and regression tests stay enabled in CI.

## Next

782: explicit TSS/RSP0/IST allocation and descriptor ownership for a new
**isolated test context**, including all actual page-table mappings and
known-good guard stack boundaries. Then carefully account for other kernel
stacks, framebuffer/GOP, MMIO, firmware runtime and cache policy. Do not
shortcut full-memory closure, W^X or hardware rollbacks.
