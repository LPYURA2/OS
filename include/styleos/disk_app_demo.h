#ifndef STYLEOS_DISK_APP_DEMO_H
#define STYLEOS_DISK_APP_DEMO_H
#include <stddef.h>
#include <stdint.h>
#include "styleos/boot_memory.h"
#include "styleos/storage.h"

/* T-P1.8B experiment, not a device-tree or public executable API. */
#define DISK_APP_MMIO_MAX 32u
enum disk_app_scenario { DISK_APP_NONE, DISK_APP_RUN, DISK_APP_REJECT };
struct disk_app_mmio { uint64_t base, bytes; };
struct disk_app_boot {
    enum disk_app_scenario scenario;
    size_t count;
    struct disk_app_mmio mmio[DISK_APP_MMIO_MAX];
};
enum disk_app_fdt_error {
    DAF_OK, DAF_ARGUMENT, DAF_HEADER, DAF_BOUNDS, DAF_FORMAT, DAF_PROFILE,
    DAF_PHASE, DAF_LIMIT
};
/* Read only; the complete output is unchanged on every refusal. */
enum disk_app_fdt_error disk_app_fdt_parse(const void *,size_t,struct disk_app_boot *);
const char *disk_app_fdt_error_name(enum disk_app_fdt_error);
const char *disk_app_scenario_name(enum disk_app_scenario);
extern _Alignas(4096) union st_workspace styleos_disk_app_storage_workspace;
void disk_app_demo_run(const struct boot_memory_evidence *);
#endif
