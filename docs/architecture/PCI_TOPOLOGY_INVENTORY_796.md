# StyleOS 796 — terminal read-only PCI hierarchy and resource-base inventory

Base 795: `2b5581f4726f47090bebe893a163f077416f9276`.
Branch: `feature/styleos-pci-topology-inventory-796`.

## Scope

795 measured **one** video BAR aperture (16 MiB) by reversible configuration
writes in disposable virtual hardware, then restored it. 796 instead performs
read-only PCI configuration DATA operations on a separate terminal QEMU/OVMF
boot. It may write the CF8 ADDRESS selector and restores it before exiting,
but never writes device configuration DATA (CFC). It does not resize BARs,
touch MMIO contents, enable IRQ, assign resources, or enter production CR3.

The test scans bus 0 and follows primary/secondary/subordinate bus routes
through discovered PCI-to-PCI bridges. An explicit visited bitmap prevents
cycles and ambiguous secondary-bus ownership; a bounded queue holds at most
256 buses. It records device identities, type-0 endpoint BAR **base addresses**,
and type-1 bridge memory / prefetchable forwarding windows. Bridge parsing is
strict for malformed bus numbers, invalid prefetchable types, and overflow;
disabled forwarding windows are reported as absent. No endpoint BAR aperture
size is inferred; `bar-sizes-unknown` must equal all observed endpoint BARs.
Only the 795 VGA BAR has a separately measured size in its own environment.

Two mandatory hardware tests are required, not interchangeable:
1. QEMU/OVMF q35 baseline and exact GOP/bar base correlation.
2. The same system with a real QEMU PCIe root port and a device downstream;
   at least two buses and at least one bridge must be observed through config
   reads. These tests use no network or access to host devices.

The host parser is independently tested under ASAN/UBSAN and COFF. The UART
verifier rejects duplicate markers, count mismatches, invalid routes, forged
GOP provenance, unknown header types, non-33 debug-exit, and authorization.

## Limitations / foundation checklist

This is not a PCI driver, generic resource allocator or production MMIO map.
Bridge forwarding windows are **observed**, not physically owned or granted.
The source of 795's BAR aperture remains one specific QEMU VGA device. Neither
PAT/MTRR effective cache policy, APIC/IRQ routing, UEFI runtime mapping, nor
complete production dependency closure has been demonstrated. The existing
Desktop kernel, loader handoff and production address space are unchanged.
Even a full SUCCESS in 796 does not finish the foundation.

Next: route dependent MMIO region inventory and cache policy, IRQ topology,
UEFI runtime closure; then isolated hardware proof of the complete desktop
mapping before any production CR3 integration. Keep the product order fixed:
foundation, Processes, independent `.tea`, user login, StyleFS/Explorer.
