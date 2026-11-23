# StyleOS 815 — FADT/DSDT and bounded static AML hints

Parent: accepted 814, `d326a035cc5fb069703bf132db0d3bb20636b219`.
Branch: `feature/styleos-acpi-static-namespace-815`.

## Boundary

The dedicated terminal x86-64 image `--test-acpi-hostbridge-815` validates
the firmware FADT checksum, length and DSDT/X_DSDT reference, then copies a
bounded checksummed DSDT into image-owned memory before ExitBootServices.
After EBS it may read PCI configuration using legacy CF8/CFC only. It does
not dereference ECAM, execute AML or write any PCI/IOMMU registers.

The model scans bounded DeviceOp candidates for PNP0A03/PNP0A08 (PCI host
bridge) and PNP0C02 (motherboard resource). It counts nearby literal
`_SEG`, `_BBN`, and static buffer `_CRS` declarations, distinguishing
`_CRS` methods from literal buffers. It may identify literal ECAM-sized
resource ranges, but does NOT establish namespace ownership.

SSDT tables are counted, not interpreted; AML ScopeOp, methods, dynamic
control flow, operation regions, namespace overlays and device ownership
are **unresolved**. A literal match is **not** ACPI _CRS/_SEG/_BBN
evaluation or proof of ECAM reservation.

The complete DSDT is serialized into 32-byte UART fragments. The host
verifier reassembles all fragments and checks offsets, byte length,
signature, checksum and SHA-256. Each run preserves original UART,
reconstructed DSDT, QEMU output, JSON and image hash.

## Safeguards

All diagnostic DMA permission functions unconditionally refuse requests.
No IOMMU DMA domain, bus mastering, DMA allow/deny, IRQ routing,
invalidation, fault handler, UEFI runtime, or production CR3 activation
is implemented by this stage. The standard kernel, existing Desktop,
UI/palette, processes, .tea and StyleFS are untouched.

## Acceptance

Only exact-head SUCCESS of the full GitHub Actions workflow counts:
- ASAN/UBSAN host parser tests; x86-64 COFF model and EFI adapter;
- 85/85 negative UART cases rejecting missing/malformed DSDT, forged
  ACPI interpretation, DMA grants and fake completion;
- actual QEMU/OVMF disabled-IOMMU, Intel VT-d, one-bridge Intel,
  two-bridge Intel and AMD-Vi if supported; each non-skipped run exits 33;
- original DSDT checksum and SHA, raw UART, full legacy regressions,
  exact HEAD source ZIP, ordinary ISO identical to accepted 814.

These are virtual hardware observations, not physical PC, Android or
VirtualBox certification.

## Afterward

The next independent step must use a suitably constrained AML namespace
interpreter with controlled OperationRegion access to actually evaluate
PCI host bridge `_CRS`/`_SEG`/`_BBN`, load relevant SSDT definitions and
correlate motherboard ECAM reservations with MCFG, UEFI RootBridge and
PCI topology. Then verify IOMMU requester aliases/exceptions and real
fault-aware DMA deny/allow before enabling any production path.

**StyleOS foundation NOT COMPLETE.**
