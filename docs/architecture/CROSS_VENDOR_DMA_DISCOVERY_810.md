# StyleOS S810 — Intel VT-d and AMD-Vi: common discovery, no DMA grants

- **Intel**: ACPI `DMAR` → `DRHD` / `RMRR` (802/808/809 baseline).
- **AMD**: ACPI `IVRS` → `IVHD` / `IVMD`, independent of CPU brand.
- ACPI is captured and validated before `ExitBootServices`; after EBS only the saved inventory is read. Multiple IVRS, invalid checksum, conflicting DMAR+IVRS are errors.
- The parser treats IVHD device entries as **opaque** until a full PCI bridge hierarchy and IVHD entry decoder exists. The 16-bit IVHD `DeviceID` alone does **not** prove ownership of all downstream PCI requesters.
- The PCI Requester ID helper builds `bus:device:function` bits and rejects invalid BDF, but does NOT resolve paths or enable DMA.
- A portable policy will need both vendor-specific backends and one common domain/lifetime/permissions API. `iommu810_allow_dma` rejects **every** request, including valid Intel/AMD inventory; there are no domain tables, IOTLB invalidations, faults or permissions.
- In CI, physical QEMU+OVMF Intel DMAR and no-IOMMU modes are **mandatory**. AMD mode runs only if `qemu-system-x86_64 -device help` includes `amd-iommu`. Otherwise `SKIPPED` with explicit version/device-list evidence, never AMD hardware PASS. An AMD IVRS host fixture under ASAN/UBSAN/COFF is mandatory even if QEMU AMD emulation is unavailable.
- Normal Desktop, ISO, native .tea, boot_info, IDT, CR3 and IRQ behavior remain unchanged.

**Unproven**: full requester path resolution, AMD real/virtual hardware when skipped, actual DMA isolation, DMA fault handling, integration into normal StyleOS, UEFI runtime and production CR3.
