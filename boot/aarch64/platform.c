#include <stdint.h>
#include "styleos/arch.h"
#include "styleos/boot_info.h"

#define PL011_BASE UINT64_C(0x09000000)
#define FDT_MAGIC UINT32_C(0xd00dfeed)

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
    return size >= 40 && size <= 16u * 1024u * 1024u &&
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
