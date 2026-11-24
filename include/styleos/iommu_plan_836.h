#ifndef STYLEOS_IOMMU_PLAN_836_H
#define STYLEOS_IOMMU_PLAN_836_H
#include <styleos/ownership.h>
#include <styleos/iommu_quarantine_835.h>
/* Detached metadata only. Caller must establish provenance and same-boot
 * relationship of both inputs. No allocation, register access or transfer. */
struct s836_work { struct op_output plan; struct boot_memory_map checked; };
int s836_extend(const struct op_output *old,const struct s835_report *q,
                struct op_output *out,struct s836_work *work);
#endif
