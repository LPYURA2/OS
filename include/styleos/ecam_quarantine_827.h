#ifndef STYLEOS_ECAM_QUARANTINE_827_H
#define STYLEOS_ECAM_QUARANTINE_827_H
#include <styleos/boot_memory.h>
/* Offline/host preflight only. No firmware calls or allocation grants.
 * Caller supplies immutable bytes of already authenticated evidence, and
 * a separate scratch object. This is not a production boot integration. */
struct ecam827_report {
    struct boot_range ecam;
    uint64_t explicit_pages, gap_pages;
    struct boot_memory_map diagnostic;
};
struct ecam827_work { struct ecam827_report candidate; };
int ecam827_quarantine(const void *mcfg, size_t mcfg_size,
    const void *drac, size_t drac_size,
    const void *map, size_t map_size, size_t stride, uint32_t version,
    struct ecam827_report *out, struct ecam827_work *work);
#endif
