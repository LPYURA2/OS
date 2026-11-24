#ifndef STYLEOS_IOMMU_GUEST_837_H
#define STYLEOS_IOMMU_GUEST_837_H
#include <styleos/iommu_plan_836.h>
#include <styleos/boot_info.h>
/* Detached same-boot diagnostics; not a memory grant. */
int s837_guest_compose(const struct boot_info *,const struct s835_report *);
#endif
