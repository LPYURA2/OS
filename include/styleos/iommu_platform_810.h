#ifndef STYLEOS_IOMMU_PLATFORM_810_H
#define STYLEOS_IOMMU_PLATFORM_810_H
#include <stdint.h>
#include <stddef.h>
/* Both x86 vendors are discovered from ACPI tables, not the CPU brand.
 * No DMA domain/translation authorization is exposed by this interface. */
#define IOMMU810_MAX_TABLE 4096u
#define IOMMU810_MAX_UNITS 16u
#define IOMMU810_MAX_REQUESTERS 64u
enum iommu810_backend { IOMMU810_REFUSED=-1, IOMMU810_ABSENT=0, IOMMU810_INTEL=1, IOMMU810_AMD=2 };
struct iommu810_unit { uint64_t mmio_base;uint16_t pci_segment;uint16_t requester_id;uint8_t type; };
struct iommu810_ivrs {
 uint32_t length,ivinfo;uint8_t revision;
 uint16_t ivhd_count,ivmd_count,unknown_blocks,opaque_entries;
 struct iommu810_unit units[IOMMU810_MAX_UNITS];
};
struct iommu810_pci { uint16_t segment;uint8_t bus,device,function; };
/* ivrs810_parse is read-only and fail-closed; IVHD device entries remain
 * unresolved until a full PCI topology walk and complete entry decoder. */
int ivrs810_parse(const uint8_t *data,size_t length,struct iommu810_ivrs *out);
enum iommu810_backend iommu810_select(int dmar_present,int dmar_valid,int ivrs_present,int ivrs_valid);
int iommu810_pci_requester(const struct iommu810_pci *pci,uint16_t *id);
int iommu810_allow_dma(enum iommu810_backend vendor,uint16_t requester_id);
#endif
