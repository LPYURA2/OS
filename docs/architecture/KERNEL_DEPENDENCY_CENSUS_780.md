# Stage 780 — read-only kernel dependency census and loaded PE W^X intent

## Scope

All new code is confined to `--test-frame-grants`, the existing independent
x86 QEMU/OVMF profile. Ordinary Desktop loads exactly as in 779. No new CR3,
no table write, no hardware driver, no IRQ, no page ownership transfer.

## Hardware evidence

After *real* hardware CR3 roundtrip and successful return to inherited UEFI
page tables, 779 rechecks CR0/CR3/CR4/EFER, the physical PTE translations
of all loaded PE pages, current stack, GDT/IDT and old page tables. 780 adds
`kernel_dependency_inventory` and reads the actual *loaded* PE32+ section
headers at the UEFI-supplied image base. The parser bounds all reads to
`LoadedImage.ImageSize`, verifies DOS and PE32+ headers, section count and
RVAs, and records page-granular intended RX/RW/RO rights. It does not
change inherited PTE rights. Mixed-write/execute pages and uncovered pages
are blockers; inconsistent PE `SizeOfImage` and firmware loaded extent is
reported as `pe-declared-extra` without accessing bytes beyond the
firmware-reported allocation.

On the 779 QEMU profile the loaded image from `STYLEOS MEMMAP IMAGE` was 159
pages, whereas PE `SizeOfImage` in the binary was 160 pages. This difference
must be measured again by CI from the 780 binary and never assumed fixed.

The live task register selector comes from `STR`, and must match the 779
hardware inventory. A nonzero selector alone never demonstrates a valid
TSS or IST stack. Current framebuffer information is not passed through
`boot_info v1` by this test profile; no GOP framebuffer or MMIO read is
attempted. UEFI runtime mappings, device memory types and final PAT/cache
settings are unproven. Legacy RWX observations from 779 are explicitly
blocking, not silently remapped.

`STYLEOS KDI 780 INHERITED` contains exact live counts and a **nonzero**
blocker bitset. Independent Python QEMU validation checks the sizes against
real PE headers, `MEMMAP IMAGE` and the inherited `KMI 779` trace, checks
all critical blockers, and rejects altered `active=1` claims.

## Security property / refusal

`kdi_audit_dependencies` is read-only and can never authorize activation;
`activation_permitted` remains false even for synthetic complete input.
The detached SAFE/User roots from 777/778 are not used. No evidence is
presented as sufficient for switching to an independently mapped CR3.

## Remaining work (781+)

- Make firmware/PE loaded extent consistent, or determine why the last PE
  page is unreported; **do not read outside the passed range**.
- Build a complete hardware provenance ledger for all kernel task and
  exception stacks (including IST), TSS, GDT/IDT and firmware/runtime memory.
- Capture GOP framebuffer and device MMIO ranges with actual cache type,
  without treating candidate RAM as device ownership.
- Materialize a W^X-safe image mapping from validated section-level roles;
  include any required dynamic relocations, exception tables and device pages.
- Only then test an isolated own-root CR3 roundtrip with explicit rollback in
  QEMU. Do not change the ordinary Desktop or declare a production PMM.
