# StyleOS: handoff after task 816

Accepted basis before 816: `b6a84c48c58434dea15aac42f4fa68ed7017451d`
(branch `feature/styleos-acpi-static-namespace-815`, exact full CI SUCCESS).
Working 816 task branch: `feature/styleos-acpi-literal-namespace-816`.

Important: 816 is a deliberately *limited AML literal-only* experiment. It
copies valid XSDT SSDTs and demonstrates `Device`/`Scope`/`Name` and
`Return(literal)` on synthetic host tests, but is NOT a complete ACPI
namespace evaluator, physical ECAM reservation proof or IOMMU ownership.
Real firmware AML with unknown code must remain incomplete. No hardware
OperationRegion access, DMA, IOMMU domains or production CR3.

Read `AGENTS.md`, `docs/PROJECT.md`,
`docs/architecture/ACPI_LITERAL_NAMESPACE_816.md`,
`docs/architecture/ACPI_STATIC_HOSTBRIDGE_815.md` and approved UI mockups.
First confirm exact HEAD SHA, final GitHub Actions SUCCESS and downloaded
CI archive, source ZIP, unchanged ordinary ISO, QEMU UART/DSDT/SSDT and SHA-256.
Do not substitute a host fixture or early intermediate CI result.

Next **independent branch**: select and safely integrate a sufficiently
complete AML interpreter, including SSDT namespace merging and deny-by-default
OperationRegion access, evaluate real hostbridge `_CRS/_SEG/_BBN`, then verify
physical ECAM reservations and IOMMU ownership/aliases/exceptions. Next
isolated real DMA deny/allow + faults and invalidations; then UEFI runtime,
owned production CR3 and full ordinary kernel+Desktop+Processes+.tea+StyleFS
file-operation integration acceptance on actual hardware.

Only after **integrated hardware acceptance** may the foundation be called
complete. Then maintain owner's order: full Processes with PID/CPU/RAM/
critical protection and tabs; independent native `.tea`; users/Welcome/Lock;
StyleFS/Explorer keeping «Быстрый доступ»; `.tea` installer/App World/network/
browser/sound/VPN.

Preserve main, accepted branches, UI/palette/mockups, history and Portable-os.
Each stage requires an exact-head successful CI ZIP, ISO, original hardware
proofs, SHA-256, technical report, continuation point and separate new-chat
message. **Foundation NOT COMPLETE.**
