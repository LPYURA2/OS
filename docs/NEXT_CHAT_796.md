# StyleOS — continuation after stage 796

Repository LPYURA2/OS; accepted baseline stage 795
`2b5581f4726f47090bebe893a163f077416f9276`.
Stage 796 branch `feature/styleos-pci-topology-inventory-796`.

**Do not treat this file as acceptance.** Verify the final SHA, full successful
GitHub Actions run, original QEMU UART from both baseline and downstream PCIe
bridge configurations, source/ISO manifests, and normal Desktop EFI invariance.
If no complete CI SUCCESS: stage remains candidate, keep 795 accepted.

This stage is a **terminal read-only PCI hierarchy inventory**. It reads PCI
configuration via CF8/CFC with CF8 saved/restored, discovers buses through
bridges, logs device/BAR bases and forwarding windows. An unknown BAR size is
never filled in from guesses; no production memory mapping, IRQ enable,
PCI resource allocation, or general driver exists. Host ASAN/UBSAN/COFF and
strict negative UART tests are separate from real QEMU.

Next investigate effective PAT/MTRR cache type, IRQ/APIC routing, UEFI runtime
ownership, and full closed production memory mapping. **Foundation remains
unfinished** until all necessary mechanisms are integrated into and verified
with normal Desktop, with no loss of existing apps/files.

Read `AGENTS.md`, `docs/PROJECT.md`, `docs/architecture/PCI_TOPOLOGY_INVENTORY_796.md`,
`docs/design/APPROVED_UI_REFERENCES.md`.

Owner roadmap: foundation -> complete real Processes task manager -> independent
native `.tea` alongside Desktop -> user selection and login per
`StyleOS-Welcome-and-Lock-Screens.pdf` -> StyleFS and Explorer per
`StyleOS-Explorer-Mockups.pdf` (section `Быстрый доступ`) -> installer, App
World, networking, browser, sound, VPN. Preserve polished Desktop/colors.
Every stage requires CI source ZIP, CI ISO, evidence, report, SHA, Actions
link, handoff, and **separate ready-to-paste new-chat message**.
