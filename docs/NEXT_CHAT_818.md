# StyleOS — handoff after experimental stage 818

Accepted base 817: ae33a3cceda1a70cb8df76cf62217eac3e655e58.
Task branch: feature/styleos-acpica-osl-guard-818.

Verify final branch HEAD and exact-head complete GitHub Actions SUCCESS,
all inherited regressions, original QEMU/OVMF UART and JSON, ZIP/ISO
checksums, and byte-identical ordinary ISO against 817 before acceptance.
The source and source-level documentation are not proof on their own.
Read AGENTS.md, docs/PROJECT.md, and
docs/architecture/ACPICA_OSL_DENY_GATE_818.md.

S818: freestanding, allocation-free ACPI table read policy plus
unconditional OperationRegion rejection, bounded guest QEMU test.
It is NOT ACPICA linked into StyleOS; upstream ACPICA was only
executed by a host tool in S817. DMA, ACPI real _CRS/_SEG/_BBN,
IOMMU controller ownership, ECAM reservation, UEFI runtime and
production CR3 are all still unproven.

Next separate branch: true ACPICA guest integration with a fully audited
OS Services Layer, then root bridge/SSDT + ECAM policy, IOMMU requesters,
real DMA deny/allow, faults and invalidation, UEFI runtime, own CR3,
finally ordinary kernel+Desktop+Processes+independent .tea+StyleFS
hardware integration. Only then call the foundation finished.

After foundation: full Processes, native .tea, users Welcome/Lock by PDF,
StyleFS Explorer by approved PDF with «Быстрый доступ», installer,
App World, Internet, browser, audio and VPN.
Never modify main, accepted branches, Desktop palette/mockups; no merges,
releases or force pushes. Every step needs full CI source ZIP, ISO,
evidence, SHA, report and next-chat message.

**Foundation NOT COMPLETE.**
