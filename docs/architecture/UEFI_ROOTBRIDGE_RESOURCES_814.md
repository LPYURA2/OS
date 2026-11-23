# StyleOS 814 — UEFI PCI root bridge resource audit

Base: accepted S813 `9e93e47a811cf7051279fe08e7670cba39bd32a2`.
Branch: `feature/styleos-uefi-rootbridge-audit-814`.
**Diagnostic only; production DMA and CR3 are denied.**

## Scope

A dedicated x86-64 UEFI terminal profile `--test-pci-rootbridge-814`
calls `EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.Configuration()` and reads its
`SegmentNumber` before ExitBootServices. It copies up to 16 firmware
resource streams of 1200 bytes each to image-owned BSS, bounded by 24
descriptors, and uses no firmware pointers or calls after EBS.

The model parses QWORD (0x8A) ACPI-format resource descriptors and EndTag
(0x79). It recognizes explicitly claimed PCI bus windows; memory and I/O
descriptors are counted without authorizing any mapping. It fails closed
on malformed bounds, duplicate bus windows, conflicting segment-zero
roots or missing bus coverage. It correlates a real CF8/CFC segment-zero
PCI topology with accepted S813 ACPI MCFG windows and the independent UEFI
PCI Root Bridge I/O bus-range report. The inherited S812 Intel/AMD requester
scope audit remains read-only.

**Critical distinction:** firmware `Configuration()` resources and
`SegmentNumber` are NOT evaluation of OS-facing ACPI namespace
`_CRS`, `_SEG`, `_BBN` or `_CBA`. The test does NOT parse AML, check
PNP0A03/PNP0A08 roots or PNP0C02 ECAM reservations, enumerate PCI
segments other than zero, or verify host bridge resource ownership. It
does not read ECAM or enable PCI bus mastering, IOMMU translation, DMA
domains, fault handling, or invalidation. `pci814_allow_dma()` always
returns zero.

### Acceptance matrix

- ASAN/UBSAN model tests with malformed and overlapping bus resources,
  missing buses, conflicting roots, immutable output-on-error and DMA denial
- x86-64 COFF compilation for both model and UEFI adapter
- Strict negative UART tests against counterfeit root-bridge/DMA claims
- Real QEMU/OVMF no-IOMMU, Intel, one-bridge Intel, two-bridge Intel and AMD
  (when available), all expected to exit with code 33, retaining original
  UART SHA256 and results JSON
- Complete inherited CI and exact-head source ZIP / normal ISO hash
  regression. QEMU is not a physical PC/VirtualBox/Termux acceptance

Documentation references:
https://uefi.org/specs/UEFI/2.10/14_Protocols_PCI_Bus_Support.html
https://docs.kernel.org/PCI/acpi-info.html

## Next

Evaluate actual ACPI namespace `_CRS`/`_SEG`/`_BBN` using a suitable
interpreter, including ECAM reservations, then prove IOMMU requester/alias
ownership and DMA deny/allow in a separate disposable-device experiment.
After UEFI runtime and production CR3, perform full kernel+Desktop+
Processes+.tea+StyleFS hardware integration.

**Foundation of StyleOS NOT COMPLETE.**
