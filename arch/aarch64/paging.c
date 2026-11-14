#include "styleos/arch.h"
#include "styleos/paging.h"

/* This file is linked only into the two T-P1.4 experiments. The old Image
 * startup and every older profile retain their MMU-off contract. */
#define ARM_IMAGE_BASE UINT64_C(0x40080000)
#define PL011_PAGE UINT64_C(0x09000000)
#define SCTLR_M UINT64_C(1)
#define SCTLR_C (UINT64_C(1) << 2)
#define SCTLR_I (UINT64_C(1) << 12)
#define SCTLR_WXN (UINT64_C(1) << 19)
#define SCTLR_E0E (UINT64_C(1) << 24)
#define SCTLR_EE (UINT64_C(1) << 25)
/* Armv8-A RES1 bits for the supported non-VHE EL1 configuration. Caches and
 * WXN stay off: the retained image mapping intentionally has mixed RWX pages.
 * This is an experiment contract, not a production kernel security policy. */
#define SCTLR_EXPERIMENT (UINT64_C(0x30d00800) | SCTLR_M)
#define MAIR_EXPERIMENT UINT64_C(0x44)

extern const unsigned char aarch64_vectors[];

static int contains(struct boot_range outer, uint64_t start, uint64_t bytes)
{
    return bytes && start >= outer.start && start < outer.end &&
           bytes <= outer.end - start;
}

static uint64_t read_sctlr(void)
{
    uint64_t value;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(value));
    return value;
}

static uint64_t read_vbar(void)
{
    uint64_t value;
    __asm__ volatile("mrs %0, vbar_el1" : "=r"(value));
    return value;
}

static void field(const char *name, uint64_t value)
{
    arch_puts(name);
    paging_hex(value);
}

static uint64_t tcr_for(unsigned ips)
{
    /* 48-bit lower VA, L0 start, 4-KiB granule, non-cacheable table walks,
     * inner-shareable lower mappings. Disable all upper-range walks (EPD1),
     * keep TTBR1 zero, no top-byte-ignore, hardware AF/dirty update or LPA2.
     * T1SZ/TG1 have valid values even though EPD1 disables that half. */
    return UINT64_C(16) | (UINT64_C(3) << 12) |
           (UINT64_C(16) << 16) | (UINT64_C(1) << 23) |
           (UINT64_C(2) << 30) | ((uint64_t)ips << 32);
}

static enum pg_error physical_profile(uint64_t mmfr0, unsigned *bits,
                                       unsigned *ips)
{
    static const unsigned widths[] = {32, 36, 40, 42, 44, 48};
    unsigned parange = (unsigned)(mmfr0 & 15u);
    /* TGran4 == 0 is the original 4-KiB profile. LPA2 and wider PA formats
     * require different descriptors/TCR handling and are refused here. */
    if (((mmfr0 >> 28) & 15u) != 0 || parange >= sizeof widths / sizeof widths[0])
        return PG_PLATFORM;
    *bits = widths[parange];
    *ips = parange;
    return PG_OK;
}

enum pg_error arch_paging_prepare(struct pg_experiment *experiment)
{
    if (!experiment || !experiment->evidence || experiment->plan.arch != PG_ARM)
        return PG_ARGUMENT;
    const struct boot_memory_evidence *evidence = experiment->evidence;
    uint64_t el, spsel, daif, sp, mmfr0;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(el));
    __asm__ volatile("mrs %0, SPSel" : "=r"(spsel));
    __asm__ volatile("mrs %0, DAIF" : "=r"(daif));
    __asm__ volatile("mov %0, sp" : "=r"(sp));
    __asm__ volatile("mrs %0, id_aa64mmfr0_el1" : "=r"(mmfr0));
    uint64_t sctlr = read_sctlr(), vbar = read_vbar();
    experiment->before_mode = sctlr;
    experiment->old_root = 0; /* No active stage-1 root when SCTLR.M == 0. */
    arch_puts("STYLEOS PAGING ARM BEFORE");
    field(" mmfr0=", mmfr0);
    field(" sctlr=", sctlr);
    field(" vbar=", vbar);
    field(" daif=", daif);
    arch_putc('\n');

    unsigned bits, ips;
    enum pg_error error = physical_profile(mmfr0, &bits, &ips);
    (void)ips;
    if (error != PG_OK) return error;
    if (el != 4 || spsel != 1 || (daif & UINT64_C(0x3c0)) != UINT64_C(0x3c0) ||
        (sp & 15u) || !contains(evidence->live.stack, sp, 16) ||
        (sctlr & (SCTLR_M | SCTLR_C | SCTLR_I | SCTLR_EE | SCTLR_E0E)))
        return PG_PLATFORM;
    if (evidence->live.image.start != ARM_IMAGE_BASE ||
        evidence->live.image.end <= evidence->live.image.start ||
        evidence->live.image.end > (UINT64_C(1) << bits) ||
        (vbar & 2047u) || vbar != (uint64_t)(uintptr_t)aarch64_vectors ||
        !contains(evidence->live.image, vbar, 2048))
        return PG_CONTEXT;

    /* In this checked raw-Image profile the current MMU is off, the complete
     * ELF VMA/LMA/Image extent is proved by the build verifier, and the Image
     * was loaded at its fixed physical address. These known object addresses
     * therefore name their physical backing. This is not a general VA->PA
     * service and does not authorize arbitrary pointers or candidate RAM. */
    uint64_t tables = (uint64_t)(uintptr_t)styleos_paging_tables;
    uint64_t data = (uint64_t)(uintptr_t)styleos_paging_data;
    if ((tables | data) & (PG_PAGE - 1u)) return PG_ALIGNMENT;
    if (!contains(evidence->live.image, tables, PG_TABLE_BYTES) ||
        !contains(evidence->live.image, data, PG_DATA_BYTES))
        return PG_RANGE;
    if (experiment->plan.count >= PG_MAX_MAPS) return PG_CAPACITY;
    for (size_t n = 0; n < experiment->plan.count; ++n) {
        struct pg_mapping map = experiment->plan.maps[n];
        if (map.va <= PL011_PAGE && PL011_PAGE - map.va < map.bytes)
            return PG_CONFLICT;
    }
    experiment->plan.pa_bits = bits;
    experiment->plan.tables_pa = tables;
    experiment->data_pa = data;
    experiment->plan.maps[experiment->plan.count++] = (struct pg_mapping){
        PL011_PAGE, PL011_PAGE, PG_PAGE, PG_WRITE | PG_DEVICE
    };
    return PG_OK;
}

enum pg_error arch_paging_activate(struct pg_experiment *experiment)
{
    /* Refusal never writes a translation register. A successful builder is
     * necessary but not enough: recheck the stable input mode and whole root
     * object immediately before publishing it to TTBR0. */
    if (!experiment || !experiment->evidence ||
        experiment->result.error != PG_OK || !experiment->result.root ||
        !experiment->result.used || experiment->result.used > PG_TABLE_PAGES ||
        experiment->result.root != experiment->plan.tables_pa ||
        experiment->plan.tables_pa != (uint64_t)(uintptr_t)styleos_paging_tables ||
        read_sctlr() != experiment->before_mode)
        return PG_CONTEXT;
    uint64_t mmfr0, el, daif;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(el));
    __asm__ volatile("mrs %0, DAIF" : "=r"(daif));
    __asm__ volatile("mrs %0, id_aa64mmfr0_el1" : "=r"(mmfr0));
    unsigned bits, ips;
    enum pg_error error = physical_profile(mmfr0, &bits, &ips);
    if (error != PG_OK) return error;
    uint64_t vbar = read_vbar();
    if (el != 4 || (daif & UINT64_C(0x3c0)) != UINT64_C(0x3c0) ||
        bits != experiment->plan.pa_bits ||
        (experiment->before_mode & (SCTLR_M | SCTLR_C | SCTLR_I)) ||
        vbar != (uint64_t)(uintptr_t)aarch64_vectors)
        return PG_PLATFORM;

    /* Verify critical actual leaves, not merely the input mapping requests.
     * The generic caller verifies every declared page and the unmapped probe;
     * this additionally refuses a missing table-storage or vector mapping. */
    struct pg_lookup lookup;
    for (uint64_t at = experiment->plan.tables_pa;
         at < experiment->plan.tables_pa + PG_TABLE_BYTES; at += PG_PAGE) {
        error = pg_lookup(&experiment->plan, at, &lookup);
        if (error != PG_OK || lookup.pa != at || !(lookup.flags & PG_WRITE) ||
            (lookup.flags & PG_DEVICE)) return PG_CONTEXT;
    }
    error = pg_lookup(&experiment->plan, vbar, &lookup);
    if (error != PG_OK || lookup.pa != vbar || !(lookup.flags & PG_EXEC) ||
        (lookup.flags & PG_DEVICE)) return PG_CONTEXT;
    error = pg_lookup(&experiment->plan, PL011_PAGE, &lookup);
    if (error != PG_OK || lookup.pa != PL011_PAGE ||
        lookup.flags != (PG_WRITE | PG_DEVICE)) return PG_CONTEXT;

    uint64_t root = experiment->result.root;
    uint64_t tcr = tcr_for(ips), mair = MAIR_EXPERIMENT;
    uint64_t sctlr = SCTLR_EXPERIMENT;
    __asm__ volatile(
        "dsb sy\n"
        "msr mair_el1, %0\n"
        "msr tcr_el1, %1\n"
        "msr ttbr0_el1, %2\n"
        "msr ttbr1_el1, xzr\n"
        "isb\n"
        "tlbi vmalle1\n"
        "dsb sy\n"
        "isb\n"
        "msr sctlr_el1, %3\n"
        "isb\n"
        :: "r"(mair), "r"(tcr), "r"(root), "r"(sctlr) : "memory");

    uint64_t ttbr0, ttbr1, actual_tcr, actual_mair;
    __asm__ volatile("mrs %0, ttbr0_el1" : "=r"(ttbr0));
    __asm__ volatile("mrs %0, ttbr1_el1" : "=r"(ttbr1));
    __asm__ volatile("mrs %0, tcr_el1" : "=r"(actual_tcr));
    __asm__ volatile("mrs %0, mair_el1" : "=r"(actual_mair));
    uint64_t actual_sctlr = read_sctlr(), actual_vbar = read_vbar();
    uint64_t actual_spsel, actual_sp, actual_daif;
    __asm__ volatile("mrs %0, SPSel" : "=r"(actual_spsel));
    __asm__ volatile("mov %0, sp" : "=r"(actual_sp));
    __asm__ volatile("mrs %0, DAIF" : "=r"(actual_daif));
    experiment->after_mode = actual_sctlr;
    arch_puts("STYLEOS PAGING ARM AFTER");
    field(" ttbr0=", ttbr0);
    field(" ttbr1=", ttbr1);
    field(" tcr=", actual_tcr);
    field(" mair=", actual_mair);
    field(" sctlr=", actual_sctlr);
    field(" vbar=", actual_vbar);
    arch_putc('\n');
    if (ttbr0 != root || ttbr1 || actual_tcr != tcr || actual_mair != mair ||
        (actual_sctlr & (SCTLR_M | SCTLR_C | SCTLR_I | SCTLR_WXN |
                         SCTLR_E0E | SCTLR_EE)) != SCTLR_M ||
        actual_vbar != vbar || actual_spsel != 1 || (actual_sp & 15u) ||
        !contains(experiment->evidence->live.stack, actual_sp, 16) ||
        (actual_daif & UINT64_C(0x3c0)) != UINT64_C(0x3c0))
        return PG_PLATFORM;
    return PG_OK;
}
