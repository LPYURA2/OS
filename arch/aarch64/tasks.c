#include "styleos/tasks.h"

extern const unsigned char image_start[], __image_end[], aarch64_vectors[];

static int inside(struct boot_range range, uint64_t start, uint64_t bytes)
{
    return range.start < range.end && bytes && start >= range.start &&
           start < range.end && bytes <= range.end - start;
}

enum tasks_error tasks_arch_observe(const struct boot_memory_evidence *e,
                                   struct tasks_arch_state *out)
{
    if (!e || !out || !e->map) return TASKS_ARGUMENT;
    uint64_t el, spsel, sp, mmfr0, sctlr, vbar, daif;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(el));
    __asm__ volatile("mrs %0, SPSel" : "=r"(spsel));
    __asm__ volatile("mov %0, sp" : "=r"(sp));
    __asm__ volatile("mrs %0, id_aa64mmfr0_el1" : "=r"(mmfr0));
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    __asm__ volatile("mrs %0, vbar_el1" : "=r"(vbar));
    __asm__ volatile("mrs %0, DAIF" : "=r"(daif));
    static const unsigned widths[] = {32, 36, 40, 42, 44, 48};
    const unsigned index = (unsigned)(mmfr0 & 15u);
    if (index >= sizeof widths / sizeof widths[0] || el != 4u || spsel != 1u ||
        daif != UINT64_C(0x3c0) ||
        (sctlr & ((UINT64_C(1) << 25) | (UINT64_C(1) << 24) |
                  (UINT64_C(1) << 12) | 5u))) return TASKS_PLATFORM;
    if ((sp & 15u) || !inside(e->live.stack, sp, 16u)) return TASKS_STACK;
    if (e->live.image.start != UINT64_C(0x40080000) ||
        e->live.image.start != (uint64_t)(uintptr_t)image_start ||
        e->live.image.end != (uint64_t)(uintptr_t)__image_end ||
        e->live.image.end > (UINT64_C(1) << widths[index]) ||
        vbar != (uint64_t)(uintptr_t)aarch64_vectors || (vbar & 2047u) ||
        !inside(e->live.image, vbar, 2048u) ||
        !inside(e->live.image, e->live.image.start, 64u) ||
        !inside(e->live.stack, e->live.handoff.start, sizeof(struct boot_info)) ||
        e->live.handoff.end - e->live.handoff.start != sizeof(struct boot_info))
        return TASKS_PLATFORM;
    /* The fixed Image executes with translation disabled.  Its linker/header
     * extent is the known identity basis, not a cast of an arbitrary pointer.
     * No DTB, candidate RAM, page tables or MMIO is probed here. */
    const volatile uint32_t *header32 = (const volatile uint32_t *)(uintptr_t)image_start;
    const volatile uint64_t *header64 = (const volatile uint64_t *)(uintptr_t)image_start;
    if (header32[14] != UINT32_C(0x644d5241) ||
        header64[2] != e->live.image.end - e->live.image.start ||
        header64[1] != UINT64_C(0x80000)) return TASKS_PLATFORM;
    const struct boot_info *info = (const struct boot_info *)(uintptr_t)e->live.handoff.start;
    if (info->arch != STYLEOS_ARCH_AARCH64 ||
        info->platform != STYLEOS_PLATFORM_QEMU_VIRT ||
        info->flags != STYLEOS_BOOT_HAS_DTB || info->dtb != e->live.source.start ||
        info->uart_base != UINT64_C(0x09000000) || info->boot_el != 1u)
        return TASKS_PLATFORM;
    *out = (struct tasks_arch_state){
        .mode = sctlr, .root = 0, .control0 = el, .control1 = spsel,
        .flags = daif, .idt_base = vbar, .idt_bytes = 2048u, .pa_bits = widths[index]};
    return TASKS_OK;
}

int tasks_arch_unchanged(const struct tasks_arch_state *a,
                         const struct tasks_arch_state *b)
{
    /* All DAIF bits, the active stack-selection mode and VBAR are invariant.
     * NZCV is caller-clobbered and is not part of this integer context. */
    return a && b && a->mode == b->mode && !a->root && !b->root &&
        a->control0 == 4u && b->control0 == 4u &&
        a->control1 == 1u && b->control1 == 1u &&
        a->flags == UINT64_C(0x3c0) && a->flags == b->flags &&
        a->idt_base == b->idt_base && a->idt_bytes == b->idt_bytes &&
        a->cs == b->cs && a->gdt_base == b->gdt_base && a->gdt_bytes == b->gdt_bytes &&
        a->pat == b->pat && a->pa_bits == b->pa_bits && a->gdt_pages == b->gdt_pages &&
        a->gdt_pa0 == b->gdt_pa0 && a->gdt_pa1 == b->gdt_pa1;
}
