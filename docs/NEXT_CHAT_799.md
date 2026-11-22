# StyleOS — continuation after stage 799

Start from accepted stage 798 commit `1b3bfec10494660fc27615659af5c5d4cdaf8e60`.
Stage 799 branch: `feature/styleos-downstream-bar-measurement-799`.
**Before accepting 799**, obtain the exact final SHA and check complete
GitHub Actions SUCCESS + raw stage 799 QEMU UART and original source/ISO hashes.
Never infer hardware PASS from a local build or a synthetic UART fixture.

Stage 799 builds a terminal x86-64 UEFI image measuring BAR1 32-bit MMIO
and BAR4 64-bit prefetchable MMIO of the quiescent QEMU VirtIO Serial
endpoint behind bridge 00:1c.0, secondary bus 1. It disables decode,
sizes, restores both original BARs and COMMAND and demands full
aperture coverage by the relevant bridge windows. No production code runs
this; no production CR3 authorization.

Next stage 800: investigate **effective** PAT/MTRR caching, IRQ/APIC,
DMA/IOMMU and runtime dependencies. Reconcile with kernel resource ledger
before an isolated Desktop integration gate. Foundation still incomplete
until actual Desktop and native applications/files operate with kernel-owned
safe memory under QEMU, without regressing earlier stages.

Agreed roadmap: finish foundation -> fully working Processes -> independent
native .tea -> users/login per approved mockups -> StyleFS + Explorer per
20-screen approved PDF ('Быстрый доступ') -> installer, App World,
network/browser/audio/VPN. Do not modify main, accepted refs, polished
Desktop or approved mockups. Every new stage its own branch, real CI,
full source ZIP, CI ISO, evidence, report, continuation and standalone new-chat
message. Read AGENTS.md, docs/PROJECT.md, this file,
docs/architecture/PCI_DOWNSTREAM_BAR_MEASUREMENT_799.md and
docs/design/APPROVED_UI_REFERENCES.md.
