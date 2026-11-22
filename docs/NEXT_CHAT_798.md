# StyleOS — continuation after stage 798

Base 797: `f07cf08c95da7175bac6dcf6cc7b03c5bb45022d`.
Stage 798 branch: `feature/styleos-pci-mmio-extent-attestation-798`.
**Verify final SHA, GitHub Actions completed SUCCESS and original evidence before acceptance.**

798 adds conservative same-CI hardware evidence reconciliation: S795 VGA BAR0
was measured and restored in its own QEMU, `0x80000000..0x81000000` 16 MiB.
S796 inventories 8/10 BAR bases in **different** QEMU boots; their sizes
remain unknown even though VGA identity/base corresponds to S795. S797
bridged forwarding base checks persist. Two downstream bridge ranges give
2 MiB and 512 MiB maximum window room **not BAR sizes**. Stage 798
asserts no MMIO or production CR3 mapping authorization and checks source
hashes, identity, bounds, restoration and fabricated evidence refusals.

Next 799: independently measure selected downstream endpoint apertures in an
isolated terminal QEMU, with device quiescence, disabling decode before the
mask probe, checking restoration, and verifying the *entire* BAR through every
ancestor bridge window. Do not size BAR live in Desktop. Also plan effective
PAT/MTRR, IRQ/APIC, DMA/UEFI runtime and eventual real Desktop integration.

**Foundation incomplete**: terminal hardware demonstrations alone do not
establish a safe production MMU/PCI map or normal Desktop+files+apps survival.

User roadmap: foundation -> real Processes (PID/CPU/RAM/details) -> separate
independent native .tea -> login/users per approved welcome/lock mockups ->
StyleFS and Explorer per 20-screen PDF ('Быстрый доступ') -> installer,
App World, network, browser, audio and VPN. Preserve current Desktop and
palette, main and accepted refs. Deliver successful CI source ZIP and ISO,
evidence, report, SHA, Actions, handoff and standalone new-chat message.

Read AGENTS.md, docs/PROJECT.md,
docs/architecture/PCI_MMIO_EXTENT_CENSUS_798.md,
docs/design/APPROVED_UI_REFERENCES.md first.
