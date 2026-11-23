# StyleOS 823 — guarded real firmware PCI0 namespace, isolated QEMU only

Accepted base: S822 `c5e7a3c36b6fc1129851edc50f6e1d0eddd71f3f`.
Branch `feature/styleos-acpica-real-hostbridge-823`.

## Goal and exact evidence boundary

S822 proved real upstream `AcpiEvaluateObject()` only on a synthetic DSDT and
SSDT. S823 moves to verified **actual QEMU/OVMF firmware DSDT/SSDT bytes**,
copied into image-owned BSS **before ExitBootServices** by the existing S816
capture path, with original table checksums and strict size bounds.

After EBS, S823 constructs only an image-local RSDP, XSDT and hardware-reduced
FADT pointing to those copies. The official pinned ACPICA 20260930 library
then loads that namespace in a **disposable terminal guest**, and evaluates
ONLY these explicit paths:

- `\\_SB.PCI0._HID`, with a canonical PNP0A08 or PNP0A03 HID accepted;
- `\\_SB.PCI0._CRS`, requiring a bounded returned ACPI buffer;
- `\\_SB.PCI0._SEG` and `\\_SB.PCI0._BBN` when present, with missing
  entries reported as **absent**, not falsely as evaluated zero.

Even successfully evaluated `_CRS` is **not** interpreted as an ECAM grant.
No `_CBA` or arbitrary AML is evaluated. The UEFI RootBridge protocol and
MCFG/PCI observations from earlier stages remain independent.

## Safety limits

- Only image-local ACPI table snapshot ranges can be mapped. All arbitrary
  physical memory/MMIO/ECAM, SystemIO, and PCI configuration callbacks deny.
- Freestanding OSL has a bounded 4-MiB monotonic arena, fixed allocations,
  a single CPU, interrupts disabled, no generic scheduler or reclamation.
- The firmware AML itself is not a trusted input. This restricted QEMU
  diagnostic does not establish safe production interpretation of arbitrary
  firmware AML; no arbitrary hardware or physical PC execution is approved.
- Real hardware DMA `deny/allow`, requester ownership, IOMMU fault handling,
  invalidation, UEFI runtime services, and production CR3 are **not proven**.
- Captured BSS snapshots are **not hardware write-protected**. Treat the
  snapshot as image-owned, not as a ROM/security isolation boundary.
- Only official upstream ACPICA 20260930 verified by pinned SHA-256, choosing
  BSD-3-Clause with attribution/disclaimer retained in CI artifacts.

## Acceptance

Required: exact-SHA full CI SUCCESS; five real QEMU/OVMF configurations (AMD
where supported), `debug-exit 33`, originals for UART and DSDT, `_HID` and
`_CRS` native ACPICA evaluation, explicit `_SEG/_BBN` presence or absence,
negative forgery checks and old regressions; unchanged normal Desktop/ISO;
full source ZIP, source hash, evidence archive, independent audit. No CI
success may be claimed before the exact run completes.

## What follows

Before taking resource ownership: fully parse/check returned ACPI resource
buffers, correlate resource windows with UEFI Root Bridge I/O + MCFG/ECAM,
account for segment and bus defaults/absence, reject overlapping or unowned
ranges, then integrate audited native OSL with fault isolation. IOMMU ownership
and real DMA tests still follow. Finally UEFI runtime, production CR3 and
joint **ordinary** kernel+Desktop+Processes+native `.tea`+StyleFS acceptance.

User-approved product plan is unchanged. **STYLEOS FOUNDATION NOT COMPLETE.**
