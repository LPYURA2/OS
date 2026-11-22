# StyleOS — continuation after stage 795

Accepted baseline 794: `0a1ebcb9d491352a9559689a31126a599ceec1d6`,
Actions 45045137427 PASS.
Current isolated branch: `feature/styleos-pci-bar-aperture-795`.

Read AGENTS.md, docs/PROJECT.md, docs/architecture/PCI_BAR_APERTURE_795.md,
docs/design/APPROVED_UI_REFERENCES.md and this file. Before accepting 795,
verify final exact SHA, full CI SUCCESS and genuine QEMU UART showing sizing
mask, measured aperture, entire GOP coverage, original BAR/PCI command restored,
QEMU exit 33; verify source ZIP/ISO and normal EFI identity to accepted 794.

795 uses TEMPORARY PCI BAR CONFIG WRITES only in a terminal QEMU test after
disabling decode. It is not read-only and not a production driver. Remaining:
PCI bridge inventory, other MMIO ranges, PAT/MTRR effective caching, IRQ/APIC,
UEFI runtime, production CR3; independent .tea alongside Desktop unproven.
The foundation is NOT FINISHED until integrated Desktop proofs succeed.

Approved product plan: foundation -> real Processes -> independent native
.tea -> users/login/lock per StyleOS-Welcome-and-Lock-Screens.pdf -> StyleFS
and Explorer per StyleOS-Explorer-Mockups.pdf ('Быстрый доступ'), then installer,
App World, network/browser/audio/VPN. Preserve working Desktop/palette.
Next stage new branch, no main/accepted edits. Deliver full CI source/ISO,
evidence, SHA/Actions, report, handoff AND separate ready-to-paste message.
