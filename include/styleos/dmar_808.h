#ifndef STYLEOS_DMAR_808_H
#define STYLEOS_DMAR_808_H
#include <stdint.h>
#include <stddef.h>
/* Inventory only, no IOMMU register writes or DMA authorization. */
#define DMAR808_MAX_DRHD 16u
#define DMAR808_MAX_RMRR 16u
#define DMAR808_MAX_TABLE 4096u
struct dmar808_drhd { uint16_t segment; uint16_t scope_count; uint8_t flags; uint64_t base; };
struct dmar808_rmrr { uint16_t segment; uint16_t scope_count; uint64_t base; uint64_t limit_inclusive; };
struct dmar808_info {
 uint32_t length;uint8_t revision,host_address_width,flags;
 uint16_t drhd_count,rmrr_count,unknown_count;
 struct dmar808_drhd drhd[DMAR808_MAX_DRHD];
 struct dmar808_rmrr rmrr[DMAR808_MAX_RMRR];
};
int dmar808_get_info(struct dmar808_info *out);
int dmar808_parse(const uint8_t *p,size_t len,struct dmar808_info *out);
#endif
