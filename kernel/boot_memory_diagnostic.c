#include "styleos/arch.h"
#include "styleos/boot_memory.h"

/* In the image's BSS, not in any reported candidate RAM or boot stack. */
static struct {
    struct boot_memory_map map;
    struct boot_memory_evidence proof;
} diagnostic;

static const char *reason(enum boot_memory_error error)
{
    switch (error) {
    case BM_RANGE: return "range";
    case BM_OVERFLOW: return "overflow";
    case BM_LIMIT: return "limit";
    case BM_CONFLICT: return "conflict";
    case BM_EMPTY: return "empty";
    case BM_UEFI_FORMAT: return "uefi-format";
    case BM_FDT_FORMAT: return "fdt-format";
    case BM_FDT_UNSUPPORTED: return "fdt-unsupported";
    case BM_METADATA: return "metadata";
    case BM_OK: break;
    }
    return "unknown";
}

static void put_hex(uint64_t value)
{
    arch_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4)
        arch_putc("0123456789abcdef"[(value >> shift) & 15u]);
}

static void print_range(const char *kind, struct boot_range range)
{
    arch_puts("STYLEOS MEMMAP ");
    arch_puts(kind);
    arch_putc(' ');
    put_hex(range.start);
    arch_putc(' ');
    put_hex(range.end);
    arch_putc('\n');
}

static int contains(struct boot_range outer, struct boot_range inner)
{
    return inner.start < inner.end && outer.start <= inner.start &&
           inner.end <= outer.end;
}

static enum boot_memory_error reserve(struct boot_range r)
{
    if (r.start >= r.end) return BM_METADATA;
    return bm_add(&diagnostic.map, r.start, r.end - r.start, 1);
}

const struct boot_memory_evidence *boot_memory_diagnose(const struct boot_info *info)
{
    struct boot_memory_live live = {0};
    struct boot_range working;
    enum boot_memory_error e = boot_memory_collect(info, &diagnostic.map, &live);
    if (!e) e = bm_extent((uintptr_t)&diagnostic, sizeof(diagnostic), &working);
    if (!e && (!contains(live.image, live.handoff) ||
               !contains(live.image, live.stack) ||
               !contains(live.image, working))) e = BM_METADATA;
    if (!e) e = reserve(live.image);
    if (!e) e = reserve(live.handoff);
    if (!e) e = reserve(live.source);
    if (!e) e = bm_normalize(&diagnostic.map);
    if (e) {
        arch_puts("STYLEOS MEMMAP ERROR: ");
        arch_puts(reason(e));
        arch_putc('\n');
        arch_halt();
    }

    arch_puts("STYLEOS MEMMAP DIAGNOSTIC ONLY\n");
    print_range("IMAGE", live.image);
    print_range("STACK", live.stack);
    print_range("HANDOFF", live.handoff);
    print_range("SOURCE", live.source);
    for (size_t i = 0; i < diagnostic.map.nr_ram; ++i)
        print_range("RAM", diagnostic.map.ram[i]);
    for (size_t i = 0; i < diagnostic.map.nr_reserved; ++i)
        print_range("RESERVED", diagnostic.map.reserved[i]);
    for (size_t i = 0; i < diagnostic.map.nr_candidates; ++i)
        print_range("CANDIDATE", diagnostic.map.candidates[i]);
    diagnostic.proof.live = live;
    diagnostic.proof.workspace = working;
    diagnostic.proof.map = &diagnostic.map;
    arch_puts("STYLEOS MEMMAP PASS\n");
    return &diagnostic.proof;
}
