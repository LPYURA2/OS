# StyleOS — stage 808 handoff

- Parent accepted stage 807: `62155dafebf658f3d6905121cd22228b96a3d466`.
- Branch: `feature/styleos-dmar-iommu-discovery-808`.
- New isolated read-only ACPI DMAR diagnostic `--test-dmar-808`.
- Two mandatory QEMU/OVMF variants: default Q35 (DMAR absent) and
  Q35 with Intel IOMMU (DMAR present). No enabling DMA translation.
- Artifacts include both original UART logs, exit codes, ASAN/UBSAN+COFF,
  negative tests and SHA256. CI success required before acceptance.
- DMA is not isolated; device scopes, DMA memory ownership, IRQ remapping,
  firmware runtime and production CR3 still unverified.
- Next S809: IOMMU DRHD MMIO capability observation / device scopes;
  UEFI runtime then own CR3 and normal Desktop integration tests.
- Approved work order: foundation → Processes → independent .tea →
  login/lock → StyleFS/Explorer → App World/network/browser/audio/VPN.
- Do not modify main or accepted branches, Desktop, design mocks.
