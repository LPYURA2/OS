# StyleOS — handoff after diagnostic stage 812

## Exact point of continuation

- Accepted and immutable baseline: S811
  `16f03f5f182592f70b3271358cfcd9e8ab2e46c2`.
- Working task branch: `feature/styleos-pci-bridge-ivhd-812`.
- S812 consists of read-only bridge enumeration, bounded ACPI DMAR
  multi-hop matching, AMD IVHD extended/alias entries and IVMD exception
  inventory. The new terminal path is `--test-pci-requester-812`.
- Authoritative record and limitations:
  `docs/architecture/PCI_BRIDGE_IVHD_812.md`.
- Acceptance requires full **exact-head** GitHub Actions SUCCESS and direct
  verification of attached source archive, normal ISO, UART, JSON and SHA256.
  A previous passing run does not certify subsequent document commits.

## What is *not* established by S812

Complete IOMMU device ownership, independent PCI segments, complete AMD
IVHD semantics, DMA isolation, DMA domains, deny/allow transactions,
invalidation, fault reporting, production CR3, UEFI runtime and normal
kernel/GUI integration have **not** been proven.

Do not infer permissions from an ACPI match, PCI bridge scope, alias,
include-all or firmware exception. DMA always denied in the diagnostic
model; no-IOMMU fallback never grants to an untrusted device.

## Work sequence

1. Check the current branch HEAD and full CI (not just first S812 steps).
   Obtain source ZIP, ISO, raw QEMU logs, checksum records and output of
   the negative and sanitized host tests. Fix any failure only on this
   task branch and rerun full CI.
2. If 812 is accepted, next distinct branch starts from its exact final
   accepted commit. Plan bounded PCI segment/ECAM/MCFG and complete
   requester-unit ownership proof if gaps remain.
3. In a distinct DMA-domain trial, use explicitly disposable QEMU
   devices and buffers; prove hardware DMA-deny then allow, faults and
   invalidations, ownership and fail-closed recovery before enabling any
   production path. No arbitrary device access.
4. After DMA protection, implement and verify UEFI runtime and own
   production CR3 with the established mapping/cache invariants.
5. Complete a full real hardware integration smoke of ordinary kernel +
   existing Desktop + Processes + independent native .tea + StyleFS
   and file operations. Only **after** integrated acceptance tell the
   owner that the whole foundation is complete.
6. Then implement full Processes (PID/CPU/RAM/state/protection/tabs),
   independent .tea, user selection/login/lock by the approved PDF,
   StyleFS/Explorer by approved PDF keeping “Быстрый доступ”, then
   .tea installer, App World, network, browser, audio and VPN.

## Preservation constraints

Never change `main`, any previously accepted branch, approved mockups,
Desktop color scheme or layout. Separate every stage into its own branch.
Keep `LPYURA2/Portable-os` untouched. Every accepted stage must provide
CI-success full source ZIP, ISO, physical/VM evidence, exact SHA, action run,
technical report and a stand-alone message for the next chat.

**The StyleOS foundation remains NOT COMPLETE.**
