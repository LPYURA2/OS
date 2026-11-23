# StyleOS 819 — test-only ACPICA-style guest OSL table mapping gate

Base: accepted S818 `1fa46a89239c5faabd1a91b6ca85ceef80866b4d`.
Branch: `feature/styleos-acpica-guest-table-gate-819`.

## Narrow security result

The new dedicated `--test-acpica-bridge-819` x86-64 UEFI diagnostic builds
`kernel/acpica_bridge_819.c` into the **actual freestanding guest**. The bridge
exports the ACPICA-style `AcpiOsMapMemory`, `AcpiOsUnmapMemory`, read/write
memory, port and PCI configuration callback subset. It is a **subset and test
harness, not a complete ACPICA OSL**. No ACPICA interpreter core is linked;
no AML instruction is executed by this guest. Do not confuse the external
ACPICA AcpiExec replay from S817 with a native ACPICA runtime.

The UEFI adapter remembers the original firmware physical addresses of verified
DSDT and SSDT tables and takes separate image-owned copies before EBS. Only
these bounded, checksummed snapshots may be mapped by callback. Mapping a
partial table must remain within one registered extent. Ambiguous overlapping
firmware ranges, invalid signatures/lengths/checksums, duplicate DSDT,
excessive SSDTs and stale generation invalidate the request. Original
firmware addresses are never dereferenced by the callback post-EBS.

Every ACPICA-style hardware read or write of MMIO/SystemMemory, SystemIO,
PCI config returns AE_ACCESS (or AE_BAD_PARAMETER on invalid metadata),
without consuming or modifying the output value and without executing the
corresponding CPU I/O instruction. A bounded request budget and finite map
slots prevent unbounded diagnostics. DMA is **always denied**. The adapter
cannot authorize ACPICA OperationRegion access, ECAM, IOMMU translation,
production CR3 or arbitrary DMA devices.

## Acceptance gates

- ASAN/UBSAN host tests of legal/rejected table snapshots, mapping bounds,
  overlapping physical intervals, mutating a copied table, one active gate,
  wrong generation, invalid PCI widths, and output immutability.
- Freestanding x86-64 COFF compile for model plus UEFI adapter.
- Negative UART forgery tests for six hardware denials, no arbitrary maps,
  real table counts, parent stage evidence and deny-all statements.
- Actual QEMU/OVMF: no IOMMU, Intel VT-d, Intel 1 bridge, Intel 2 bridges,
  AMD-Vi if supported, preserving raw UART/DSDT and SHA-256.
- Full inherited CI, reproducible source ZIP and unchanged normal ISO.
  Test-only EFI is *not* the normal Desktop ISO.

## Outstanding before foundation completion

**Not yet integrated:** real ACPICA source and core, complete OSL allocation,
locks, timers, interrupts, threading and namespace semantics; ACPI SSDT runtime
and safe OperationRegion policy; evaluated `_CRS`/`_SEG`/`_BBN`; ECAM resource
claims and entire PCI segment inventory; IOMMU requester ownership and DMA
hardware deny/allow/faults/invalidation; UEFI runtime; safe production CR3;
integrated regular kernel + Desktop + Processes + native .tea + StyleFS.

Use the original ACPICA `acpiosxf.h` contracts and an audited/pinned source
revision for real guest linkage. Relevant official documentation:
https://open-acpica.github.io/ and
https://github.com/open-acpica/acpica/tree/master/source/include .

**Foundation is NOT COMPLETE.**
