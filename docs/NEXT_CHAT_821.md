# StyleOS — next chat after experimental 821

Basis S820: `b4d768a522e5f5e8069c9da9f2f3d560cb16188d` (exact CI SUCCESS).
S821 branch: `feature/styleos-acpica-core-link-821`. Verify its final GitHub
head and complete successful CI, real UART/DSDT, source ZIP, normal ISO and
SHA manifests before acceptance. Initial candidate testing does not count.

S821's core compiler includes all official, pinned 20260930 ACPICA kernel
components, checks COFF symbols and produces a .lib. The diagnostic guest runs
a genuine ACPICA checksum implementation on a captured DSDT, **not** the AML
interpreter. Only referenced objects are necessarily linked into the EFI.

Next: verify real `AcpiEvaluateObject` invocation with a guarded freestanding
OS Services Layer, bounded native heap/synchronization, deterministic test
DSDT+SSDT and per-region deny-by-default. Evaluate the genuine `_CRS`/`_SEG`/
`_BBN` path, reconcile UEFI/MCFG/ECAM, then IOMMU ownership, real DMA
hardware deny/allow, fault and invalidation. After that UEFI runtime,
production CR3 and combined ordinary Desktop+kernel+Processes+.tea+StyleFS
hardware integration. **Foundation NOT COMPLETE** until that gate.

Product stages after foundation: Processes PID/CPU/RAM/protection/tabs;
independent .tea; Welcome/Lock; StyleFS/Explorer with “Быстрый доступ”;
installer, App World, network, browser, audio, VPN.

Never change main, accepted branches, Desktop UI/colors or approved mockups.
No force push, merge, release. Every stage new branch, CI-success full source
ZIP, ISO, raw hardware evidence, SHA, report, continuation, next chat handoff.
