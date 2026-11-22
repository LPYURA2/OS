#ifndef STYLEOS_OWNED_LOW_790_H
#define STYLEOS_OWNED_LOW_790_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <styleos/paging.h>

/* Terminal 790 experiment: full independent low-half 4-KiB hierarchy.
 * Firmware still supplies initial loaded PE and boot stack, but no low
 * page-table *page* is shared after CR3. No production permission grant. */
struct s790_low_request {
    struct boot_range image, boot_stack;
    const void *loaded_image;       /* validated PE bytes at image.start */
    uint64_t (*owned_tables)[512];  /* aligned, physically admitted image BSS */
    uint64_t owned_tables_pa;        /* proven identity PA, never inferred here */
    uint64_t required_exec, required_write, required_rsp;
};
struct s790_low_result {
    struct pg_plan plan;
    struct pg_result built;
    uint64_t new_low_slot;          /* points ONLY to owned lower PDPT */
    uint32_t code_pages, writable_pages, readonly_pages, stack_pages;
    bool ready;                     /* structural proof only */
};
/* Refuses mixed W+X pages, ambiguous PE metadata, gaps, overlaps, bad
 * image/stack identity inputs and table exhaustion. Does not install CR3. */
bool s790_prepare_owned_low(const struct s790_low_request *req,
                             struct s790_low_result *out);
/* Audits live (CPU A/D mutated) table pages; does not trust PASS markers. */
bool s790_audit_owned_low(const struct s790_low_result *low,
                           uint64_t current_code,uint64_t current_write,
                           uint64_t boot_rsp);
#endif
