#ifndef STYLEOS_IOMMU_FIXTURE_835_H
#define STYLEOS_IOMMU_FIXTURE_835_H
#include <styleos/iommu_quarantine_835.h>
#include <styleos/boot_info.h>
/* Only the isolated Q35 fixture; exact size is independently checked by QMP. */
int s835_input_from_tables(unsigned backend,const uint8_t *table,size_t tn,
 const uint8_t *madt,size_t an,const uint8_t *crs,size_t cn,
 const uint8_t *root,size_t rn,struct s835_input *out,struct s835_input *scratch);
int s835_guest_report(const struct boot_info *boot,const uint8_t *crs,size_t cn,
 const uint8_t *drac,size_t dn);
#endif
