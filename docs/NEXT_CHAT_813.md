# StyleOS — continuation after S813

Accepted baseline: S812
`e15d6eb84f26d0ac008122ab758df80d1a42de1b`, full CI
https://github.com/LPYURA2/OS/actions/runs/45171179496 (SUCCESS).

Task branch: `feature/styleos-mcfg-segment-audit-813`.
**Check its exact latest HEAD and the corresponding full GitHub Actions
result before treating S813 as accepted**; a prior passing run is not enough.
The authoritative technical report is
`docs/architecture/PCI_MCFG_SEGMENT_813.md`.

S813 adds a new isolated `--test-pci-mcfg-813` read-only test for validated
ACPI MCFG entries, bounded segment/bus lookup and ECAM arithmetic. It checks
for overlapping physical windows and MCFG coverage of the *observed* PCI
segment-zero bus topology, and runs the inherited Intel DMAR/AMD IVRS audit.
A synthetic host test checks multiple PCI segments and nonzero start bus.
A real PCI root-port + second bridge is tested in QEMU/OVMF. ECAM memory
is **not accessed**; full PCI segments and hostbridge `_CRS` remain unverified.

Do not infer DMA ownership or permissions from parsed MCFG. All IOMMU
DMA-domain operations, real DMA deny/allow, invalidations, fault handling,
UEFI runtime and production CR3 remain undone. The foundation remains
**NOT COMPLETE**.

Next: safe hostbridge `_CRS` + `_SEG`, vendor-unit ownership/exception coverage,
then separate actual DMA-deny/allow domains and faults in disposable QEMU.
Afterwards UEFI runtime, own production CR3, then integration of ordinary
kernel + Desktop + processes + .tea + StyleFS file operations.
Then Processes → independent native .tea → users/welcome/lock →
StyleFS/Explorer (“Быстрый доступ”) → installer/App World/network/
browser/audio/VPN.

Preserve main, all historical and accepted branches, current Desktop and
palette, approved PDF mockups, other repos, and data disks. One distinct
branch per stage. Deliver complete CI-success source ZIP, ISO, raw UART,
JSON, SHA256, Actions link, factual report and next-chat message each stage.
