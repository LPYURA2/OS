#include "styleos/boot_info.h"
#include <assert.h>
#include <stdio.h>

static struct boot_info x86(void)
{
    return (struct boot_info){
        .magic = STYLEOS_BOOT_MAGIC, .version = STYLEOS_BOOT_VERSION,
        .size = sizeof(struct boot_info), .arch = STYLEOS_ARCH_X86_64,
        .platform = STYLEOS_PLATFORM_UEFI, .flags = STYLEOS_BOOT_EXITED_SERVICES,
        .memory_map = 0x100000, .memory_map_size = 96,
        .descriptor_size = 48, .descriptor_version = 1, .uart_base = 0x3f8
    };
}

int main(void)
{
    struct boot_info b = x86();
    assert(boot_info_validate(&b, STYLEOS_ARCH_X86_64) == BOOT_INFO_OK);
    assert(boot_info_validate(NULL, STYLEOS_ARCH_X86_64) == BOOT_INFO_NULL);
#define REJECT(field, value, expected) do { b = x86(); b.field = (value); \
    assert(boot_info_validate(&b, STYLEOS_ARCH_X86_64) == (expected)); } while (0)
    REJECT(magic, 0, BOOT_INFO_MAGIC);
    REJECT(version, 2, BOOT_INFO_VERSION);
    REJECT(size, sizeof(b) - 8, BOOT_INFO_SIZE);
    REJECT(arch, STYLEOS_ARCH_AARCH64, BOOT_INFO_ARCH);
    REJECT(platform, STYLEOS_PLATFORM_QEMU_VIRT, BOOT_INFO_PLATFORM);
    REJECT(flags, 0, BOOT_INFO_FLAGS);
    REJECT(flags, STYLEOS_BOOT_EXITED_SERVICES | 0x80, BOOT_INFO_FLAGS);
    REJECT(memory_map, 0, BOOT_INFO_MEMORY_MAP);
    REJECT(memory_map, UINT64_MAX - 7, BOOT_INFO_MEMORY_MAP);
    REJECT(descriptor_size, 0, BOOT_INFO_MEMORY_MAP);
    REJECT(descriptor_size, 39, BOOT_INFO_MEMORY_MAP);
    REJECT(descriptor_version, 2, BOOT_INFO_MEMORY_MAP);
    REJECT(memory_map_size, 97, BOOT_INFO_MEMORY_MAP);
    REJECT(dtb, 0x40000000, BOOT_INFO_DTB);
    REJECT(boot_el, 1, BOOT_INFO_EL);
    REJECT(reserved[1], 1, BOOT_INFO_RESERVED);
    REJECT(uart_base, 0, BOOT_INFO_PLATFORM);
    b = (struct boot_info){
        .magic = STYLEOS_BOOT_MAGIC, .version = STYLEOS_BOOT_VERSION,
        .size = sizeof(b), .arch = STYLEOS_ARCH_AARCH64,
        .platform = STYLEOS_PLATFORM_QEMU_VIRT, .flags = STYLEOS_BOOT_HAS_DTB,
        .dtb = 0x44000000, .uart_base = 0x09000000, .boot_el = 1
    };
    assert(boot_info_validate(&b, STYLEOS_ARCH_AARCH64) == BOOT_INFO_OK);
    b.dtb = 0;
    assert(boot_info_validate(&b, STYLEOS_ARCH_AARCH64) == BOOT_INFO_DTB);
    b.dtb = 0x44000001;
    assert(boot_info_validate(&b, STYLEOS_ARCH_AARCH64) == BOOT_INFO_DTB);
    b.dtb = 0x44000000; b.boot_el = 2;
    assert(boot_info_validate(&b, STYLEOS_ARCH_AARCH64) == BOOT_INFO_EL);
    b.boot_el = 1; b.memory_map = 8;
    assert(boot_info_validate(&b, STYLEOS_ARCH_AARCH64) == BOOT_INFO_MEMORY_MAP);
    assert(boot_info_validate(&b, 99) == BOOT_INFO_ARCH);
    puts("boot_info host tests: PASS");
    return 0;
}
