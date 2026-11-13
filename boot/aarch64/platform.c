#include <stdint.h>
#include "styleos/arch.h"
#include "styleos/boot_info.h"
#include "styleos/boot_memory.h"

#define PL011_BASE UINT64_C(0x09000000)
#define FDT_MAGIC UINT32_C(0xd00dfeed)

extern const unsigned char image_start[], __image_end[];

static uint32_t read_be32(const volatile uint8_t *address)
{
    return ((uint32_t)address[0] << 24) | ((uint32_t)address[1] << 16) |
           ((uint32_t)address[2] << 8) | (uint32_t)address[3];
}

/* Validate the fixed FDT header, not the full tree or physical memory map.
 * QEMU's boot protocol owns the pointer. An inaccessible physical pointer is
 * caught by the early exception vector; this is not an untrusted-input sandbox. */
static int valid_dtb(uint64_t address)
{
    if (address == 0 || (address & 7) != 0 || address > UINT64_MAX - 40)
        return 0;
    const volatile uint8_t *header = (const volatile uint8_t *)(uintptr_t)address;
    if (read_be32(header) != FDT_MAGIC)
        return 0;
    uint32_t size = read_be32(header + 4);
    uint32_t structure = read_be32(header + 8);
    uint32_t strings = read_be32(header + 12);
    uint32_t reservations = read_be32(header + 16);
    uint32_t version = read_be32(header + 20);
    uint32_t compatible_version = read_be32(header + 24);
    uint32_t strings_size = read_be32(header + 32);
    uint32_t structure_size = read_be32(header + 36);
    return size >= 40 && size <= BOOT_DTB_MAX &&
           structure >= 40 && structure < size && (structure & 3) == 0 &&
           strings >= 40 && strings < size && reservations >= 40 &&
           reservations < size && (reservations & 7) == 0 && version >= 17 &&
           compatible_version <= 17 && strings_size <= size - strings &&
           structure_size <= size - structure;
}

_Noreturn void aarch64_bad_el(uint64_t el)
{
    (void)el;
    arch_console_init();
    arch_puts("STYLEOS AARCH64 BOOT ERROR: unsupported exception level\n");
    arch_halt();
}

_Noreturn void aarch64_exception(void)
{
    arch_puts("STYLEOS AARCH64 BOOT ERROR: early exception\n");
    arch_halt();
}

_Noreturn void aarch64_boot_main(uint64_t dtb, uint64_t initial_el)
{
    arch_console_init();
    if (initial_el == 2)
        arch_puts("STYLEOS AARCH64: initial EL2\n");
    else
        arch_puts("STYLEOS AARCH64: initial EL1\n");
    uint64_t current_el;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(current_el));
    if (current_el != 4) {
        arch_puts("STYLEOS AARCH64 BOOT ERROR: EL1 handoff required\n");
        arch_halt();
    }
    arch_puts("STYLEOS AARCH64: current EL1\n");
    if (!valid_dtb(dtb)) {
        arch_puts("STYLEOS AARCH64 BOOT ERROR: invalid DTB header\n");
        arch_halt();
    }
    arch_puts("STYLEOS AARCH64: DTB header OK\n");
    struct boot_info info = {
        .magic = STYLEOS_BOOT_MAGIC,
        .version = STYLEOS_BOOT_VERSION,
        .size = sizeof(struct boot_info),
        .arch = STYLEOS_ARCH_AARCH64,
        .platform = STYLEOS_PLATFORM_QEMU_VIRT,
        .flags = STYLEOS_BOOT_HAS_DTB,
        .dtb = dtb,
        .uart_base = PL011_BASE,
        .boot_el = 1
    };
#ifdef STYLEOS_TEST_INVALID_BOOT_INFO
    info.magic ^= UINT64_C(1);
#endif
    kernel_main(&info);
}


extern const unsigned char __stack_bottom[], __stack_top[];

enum boot_memory_error boot_memory_collect(const struct boot_info *info,
                                            struct boot_memory_map *map,
                                            struct boot_memory_live *live)
{
    live->image = (struct boot_range){(uintptr_t)image_start,
                                     (uintptr_t)__image_end};
    live->stack = (struct boot_range){(uintptr_t)__stack_bottom,
                                     (uintptr_t)__stack_top};
    enum boot_memory_error e = bm_extent((uintptr_t)info, sizeof(*info),
                                          &live->handoff);
    if (e) return e;
    const uint8_t *dtb = (const uint8_t *)(uintptr_t)info->dtb;
    uint32_t available = read_be32((const volatile uint8_t *)dtb + 4);
    struct boot_range extent;
    e = bm_extent(info->dtb, available, &extent);
    if (e) return e;
    /* The QEMU virt boot convention supplies the readable DTB. The parser
     * bounds offsets within its checked total, not arbitrary physical input. */
#ifdef STYLEOS_TEST_INVALID_MEMORY_MAP
    --available; /* Make the DTB backing range one byte too short. */
#endif
    uint32_t total = 0;
    e = bm_fdt_parse(map, dtb, available, &total);
    if (e) return e;
    live->source = extent;
    if (total != extent.end - extent.start ||
        !bm_ram_contains(map, live->source) || !bm_ram_contains(map, live->image))
        return BM_METADATA;
    /* Explicit exclusion even if a malformed platform describes MMIO as RAM. */
    return bm_add(map, PL011_BASE, BOOT_MEMORY_PAGE, 1);
}
