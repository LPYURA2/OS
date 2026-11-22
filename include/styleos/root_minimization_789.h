#ifndef STYLEOS_ROOT_MINIMIZATION_789_H
#define STYLEOS_ROOT_MINIMIZATION_789_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
/* Terminal-only PML4 pruning. Retains inherited low slot zero, and a
 * physically independent IST alias at slot 128. Lower level tables of slot
 * zero are STILL inherited: never claim full independent kernel W^X. */
#define R789_PML4_ENTRIES 512u
#define R789_LOW_SLOT_LIMIT (UINT64_C(1)<<39)
#define R789_ALIAS_SLOT 128u
/* All required image code/data/console/stack addresses are low 512GiB. */
bool r789_prune_inherited_root(uint64_t staged[512],
    const uint64_t inherited[512], const uint64_t *required,
    size_t nr_required);
bool r789_audit_restricted_root(const uint64_t staged[512],
    const uint64_t inherited[512]);
#endif
