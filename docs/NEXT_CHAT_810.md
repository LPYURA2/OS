# StyleOS — stage 810 handoff

Base: S809 `5100e470fa6533ee50c8679cb7e157883ba25d42`. Stage branch `feature/styleos-cross-vendor-iommu-810`.

S810 introduces isolated ACPI IOMMU backend detection: Intel DMAR and AMD IVRS. IVHD device entries are unresolved and *never* authorize DMA. Cross-vendor conflicts and invalid ACPI fail closed. QEMU Intel/no-IOMMU mandatory; AMD hardware only if `-device amd-iommu` is present, otherwise explicitly SKIPPED. No real AMD platform verified without a real QEMU AMD PASS, even though AMD parser host tests pass.

Before continuing read `AGENTS.md`, `docs/PROJECT.md`, `docs/architecture/CROSS_VENDOR_DMA_DISCOVERY_810.md`, and approved design references. Continue with complete PCI requester path resolution for Intel DMAR and AMD IVRS, then safe verified IOMMU domains and fault handling, UEFI runtime, production CR3 integration. Do not change Desktop, main or accepted refs.

**Foundation not complete**. Subsequent order: full Processes → native `.tea` independence → login/lock → StyleFS/Explorer → App World/network/browser/audio/VPN.
