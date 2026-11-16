#ifndef STYLEOS_STORAGE_DEMO_H
#define STYLEOS_STORAGE_DEMO_H
#include <stddef.h>
#include <stdint.h>
#include "styleos/boot_memory.h"
#include "styleos/storage.h"

/* T-P1.8A only: an exact QEMU virt experiment, not a device-tree bus API. */
#define STORAGE_MMIO_MAX 32u
#define STORAGE_DEMO_CONTROL_BYTES 4096u
enum storage_phase {
    STORAGE_PHASE_NONE, STORAGE_INIT_A, STORAGE_READ_A, STORAGE_UPDATE_B,
    STORAGE_READ_B, STORAGE_PREPARE_B, STORAGE_RECOVER_A
};
struct storage_mmio { uint64_t base, bytes; };
struct storage_boot {
    enum storage_phase phase;
    size_t count;
    struct storage_mmio mmio[STORAGE_MMIO_MAX];
};
enum storage_fdt_error {
    SF_OK, SF_ARGUMENT, SF_HEADER, SF_BOUNDS, SF_FORMAT, SF_PROFILE,
    SF_PHASE, SF_LIMIT
};
/* Pure bounded read-only parse. Output is unchanged on every refusal. */
enum storage_fdt_error storage_fdt_parse(const void *, size_t,
                                        struct storage_boot *);
const char *storage_fdt_error_name(enum storage_fdt_error);
const char *storage_phase_name(enum storage_phase);

struct storage_demo_state {
    struct st_store store;
    struct st_pending pending;
    struct st_record record;
    unsigned transport_error;
};
union storage_demo_control {
    struct storage_demo_state state;
    unsigned char bytes[STORAGE_DEMO_CONTROL_BYTES];
};
_Static_assert(sizeof(struct storage_demo_state)<=STORAGE_DEMO_CONTROL_BYTES,
               "complete storage demo state");
_Static_assert(sizeof(union storage_demo_control)==STORAGE_DEMO_CONTROL_BYTES,
               "complete storage demo control");
extern _Alignas(4096) union storage_demo_control styleos_storage_control;
extern _Alignas(4096) union st_workspace styleos_storage_workspace;
void storage_demo_run(const struct boot_memory_evidence *);
#endif
