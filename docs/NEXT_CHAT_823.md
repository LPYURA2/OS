# StyleOS 823 — exact continuation

Accepted parent S822: `c5e7a3c36b6fc1129851edc50f6e1d0eddd71f3f`,
GitHub CI SUCCESS: https://github.com/LPYURA2/OS/actions/runs/45270768614.

S823 branch: `feature/styleos-acpica-real-hostbridge-823`.
Confirm exact final GitHub commit and full CI SUCCESS before acceptance.

Target: actual QEMU firmware DSDT/SSDT copied into image-owned BSS before
EBS; real ACPICA 20260930 `AcpiEvaluateObject` only for PCI0 `_HID`, `_CRS`,
optional `_SEG/_BBN`, with zero hardware grants, no DMA/ECAM and no full
production OSL. Absent methods must be marked absent, not evaluated.

Required: QEMU UART logs, positive and negative strict verification, full
regressions, unchanged normal ISO and Desktop, exact GitHub blobs, CI source
ZIP, evidence archive and SHA-256; do not overclaim physical hardware.

Next: ACPI resource decoding and cross-check UEFI+MCFG+ECAM; DMA requester
aliases/IOMMU hardware deny/allow/fault/invalidation; UEFI runtime; production
CR3; integrated ordinary kernel + processes + independent `.tea` + StyleFS.

Only THEN proceed to final processes, FS, application separation and
VirtualBox tests. Do not change main or UI. **FOUNDATION NOT COMPLETE.**
