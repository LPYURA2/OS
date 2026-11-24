#ifndef STYLEOS_IOMMU_QUARANTINE_835_H
#define STYLEOS_IOMMU_QUARANTINE_835_H
#include <styleos/ecam_quarantine_827.h>
/* Host preflight: external evidence supplies exact, independently observed
 * byte aperture and other resource ranges. No access, allocation or ownership. */
#define S835_OTHER_MAX 256u
struct s835_input {
    unsigned backend; /* 0 absent, 1 Intel, 2 AMD; bounded QEMU fixture only */
    struct boot_range aperture;
    struct boot_range others[S835_OTHER_MAX];
    size_t nr_others;
};
struct s835_report {
    struct boot_range aperture, cover;
    uint64_t explicit_pages, gap_pages;
    struct boot_memory_map diagnostic;
};
struct s835_work { struct ecam827_work ecam_work; struct ecam827_report ecam; struct s835_report result; };
int s835_quarantine(const struct s835_input *in,
    const void *mcfg,size_t mn,const void *drac,size_t dn,
    const void *map,size_t n,size_t stride,uint32_t version,
    struct s835_report *out,struct s835_work *work);
#endif
