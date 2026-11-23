# StyleOS 820 — authentic ACPICA source and ABI admission

Base: accepted S819 f0a00c7a12f9bcf9b65dea5b711b29cc9958c370.
Branch: feature/styleos-acpica-upstream-abi-820.

The previous S819 UEFI guest exports only ACPICA-style callbacks, NOT a
linked/interpreted ACPICA AML engine. This narrow step admits actual pinned
ACPICA 20260930 source, SHA-256
aa18901b92e30749be0edc3081c8d550c61fce4fa37546fc6a65d367a4ae71a5
from the official GitHub release archive. CI verifies the archive before
extracting any source, rejects links/path traversal/duplicate and oversized
members, and compiles genuine ACPICA utxfinit.c, nsxfname.c, psparse.c
for x86-64 COFF. An official acpi.h ABI host smoke with ASAN/UBSAN confirms
the S819 public callback signatures and original deny status values.

This stage DOES NOT link or execute ACPICA inside StyleOS, implement complete
freestanding OS Services, run namespace methods, authorize OperationRegion
or DMA, access ECAM, switch production CR3 or alter the ordinary Desktop.
The source is fetched only into the isolated CI workspace and is not part
of the normal installed OS or source ZIP.

Acceptance: entire exact-head CI SUCCESS, upstream archive SHA and COFF
objects, ABI tests, negative tests, full inherited QEMU/OVMF regressions,
source ZIP, unchanged ISO and original UART evidence.

Next: full upstream ACPICA guest linkage with audited OSL, actual
method execution and SSDT, then ACPI host bridge resource consistency,
IOMMU ownership, real DMA deny/allow/faults/invalidation, UEFI runtime,
production CR3, and integrated regular kernel+Desktop+Processes+.tea+
StyleFS hardware acceptance.

**Foundation NOT COMPLETE.**
