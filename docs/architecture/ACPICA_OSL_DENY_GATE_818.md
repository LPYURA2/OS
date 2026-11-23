# StyleOS 818 — early ACPICA OSL deny-by-default substrate

Baseline: accepted S817 ae33a3cceda1a70cb8df76cf62217eac3e655e58.
Branch: feature/styleos-acpica-osl-guard-818.

## Exact scope

The distinct x86 UEFI diagnostic --test-acpi-osl-818 includes an allocation-free
freestanding kernel C policy module. It uses the image-owned validated DSDT
copy from S816, reads a fixed-width value only from that copy, checks a
generation token, bounds and a strict 256-query budget. Its table pointer
is borrowed from the validated stable image snapshot, not physically mapped
or independently duplicated; no untrusted kernel mutation protection is
claimed.

All OperationRegion reads AND writes are denied, regardless of SystemMemory,
SystemIO, PCI_Config, EmbeddedControl or vendor space. No arbitrary address
is dereferenced, no PCI/port/MMIO writes take place, and output remains
unchanged on error. DMA grants and production CR3 always remain denied.
The ordinary UEFI Desktop and boot profiles are unchanged.

## Critical limitations

This is NOT ACPICA guest linkage, NOT a complete ACPICA OS Services Layer,
NOT a working AML interpreter in StyleOS, and NOT real DMA deny/allow isolation.
S817's ACPICA AcpiExec remains host-only. The S818 guest policy is deliberately
a small preparatory security substrate for a future audited integration.
The existing S816 read-only CF8/CFC inventory is retained. No ECAM read is
performed by this policy.

Host C test with ASAN/UBSAN; COFF compilation; strict negative UART tests;
actual QEMU/OVMF debug-exit 33 across no-IOMMU, Intel, one-bridge,
two-bridge and AMD configurations; full original regression CI, exact-head
source archive and untouched ordinary ISO required for acceptance.
The independent strict S818 verifier checks additional guest policy records
and every inherited S816 hardware record. QEMU does not certify physical
PC, VirtualBox or Termux.

## Required next hardware work

Integrate and audit an actual ACPICA release in a native guest with
complete OS Services Layer, memory protection, allocation/locks,
execution limits, safe OperationRegion routing, resource ownership,
SSDT namespace, and controlled initialization. Then truly evaluate
ACPI host bridge _CRS/_SEG/_BBN and ECAM reservation, correlate UEFI/MCFG,
verify IOMMU requester aliases and exceptions, perform real isolated DMA
deny/allow with faults and invalidation. UEFI runtime, own production CR3,
and an integrated native kernel+Desktop+Processes+.tea+StyleFS file-operation
hardware smoke are required BEFORE calling the foundation complete.
Do not change main, old branches or approved UI/mockups.

**StyleOS foundation NOT COMPLETE.**
