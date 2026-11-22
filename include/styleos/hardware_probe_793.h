#ifndef STYLEOS_HARDWARE_PROBE_793_H
#define STYLEOS_HARDWARE_PROBE_793_H
#include <stdint.h>
#include <stdbool.h>
#include <styleos/boot_info.h>

/* UEFI facts observed under the inherited address space; never an authorization
 * to switch production CR3 or to assume effective PAT/MTRR policy. */
struct hw793_map_report {
    uint64_t page_start, page_end, covered_bytes, attribute_or;
    uint32_t descriptor_count, overlaps, runtime_count, matched_type;
    bool all_pages_described, type_mixed;
};
bool hw793_map_inspect(const struct boot_info *info, uint64_t base,
                       uint64_t bytes, struct hw793_map_report *out);
#endif
