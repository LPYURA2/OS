# StyleOS — stage 809 handoff

- Parent accepted S808: `e3c1ad3be854f669ef8a088f564953d71a79db17`.
- Branch: `feature/styleos-vtd-registers-809`.
- S809 isolated read-only VT-d DRHD MMIO register observation after ACPI DMAR capture, with bounded page-map validation. Two real QEMU/OVMF modes required, one with Intel IOMMU, one without.
- No change to normal Desktop, main, approved themes, UI, native .tea or boot_info v1.
- No DMA translation, no IOMMU domain, no proven device isolation; no production CR3.
- Next stage: requester IDs, scopes, safe DMA domain and faults; UEFI runtime, then safe own-CR3 integration with Desktop/apps/files.
- Accepted work order: foundation → Processes → independent .tea → login/lock → StyleFS/Explorer → installer/App World/network/browser/audio/VPN.
