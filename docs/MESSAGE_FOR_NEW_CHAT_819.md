# StyleOS — instruction for next chat after 819

Continue StyleOS `LPYURA2/OS`. Source baseline S818:
`1fa46a89239c5faabd1a91b6ca85ceef80866b4d`.
S819 branch `feature/styleos-acpica-guest-table-gate-819`.
Confirm exact branch SHA and final SUCCESS Actions, source ZIP, normal ISO,
raw QEMU/OVMF UART, SHA256 and negative tests. Read `AGENTS.md`,
`docs/NEXT_CHAT_819.md`, `docs/architecture/ACPICA_GUEST_TABLE_GATE_819.md`.
S819 is an *isolated test-only guest ACPICA callback gate*, not a linked ACPICA
interpreter. It maps only verified image-owned ACPI table copies from actual
firmware physical addresses; hardware memory/port/PCI requests return denied.
No DMA permission. The previous AcpiExec AML replay is external host-only.

Next branch: integrate actual pinned ACPICA core and full freestanding OSL into
one dedicated diagnostic guest, without any unproven MMIO/OperationRegion
access; then evaluate ACPI host bridge and IOMMU ownership. Follow through
real DMA deny/allow, fault/invalidation, UEFI runtime, safe own production CR3
and full ordinary Desktop+kernel+Processes+.tea+StyleFS integration.
**StyleOS foundation NOT COMPLETE.**
Protect main, accepted branches, UI/palette and mockups. Deliver exact SHA,
complete successful CI source ZIP, normal ISO, hardware evidence, hashes,
report, continuation and next-chat message each stage.
