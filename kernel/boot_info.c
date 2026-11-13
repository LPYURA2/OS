#include "styleos/boot_info.h"

enum boot_info_error boot_info_validate(const struct boot_info *b, uint32_t arch)
{
    if (!b) return BOOT_INFO_NULL;
    if (b->magic != STYLEOS_BOOT_MAGIC) return BOOT_INFO_MAGIC;
    if (b->version != STYLEOS_BOOT_VERSION) return BOOT_INFO_VERSION;
    if (b->size != sizeof(*b)) return BOOT_INFO_SIZE;
    if ((arch != STYLEOS_ARCH_X86_64 && arch != STYLEOS_ARCH_AARCH64) ||
        b->arch != arch) return BOOT_INFO_ARCH;
    if (b->reserved0 || b->reserved[0] || b->reserved[1])
        return BOOT_INFO_RESERVED;
    if (arch == STYLEOS_ARCH_X86_64) {
        if (b->platform != STYLEOS_PLATFORM_UEFI) return BOOT_INFO_PLATFORM;
        if (b->flags != STYLEOS_BOOT_EXITED_SERVICES) return BOOT_INFO_FLAGS;
        if (!b->memory_map || (b->memory_map & 7) ||
            b->descriptor_size < 40 || b->descriptor_size > 4096 ||
            (b->descriptor_size & 7) || b->descriptor_version != 1 ||
            !b->memory_map_size || b->memory_map_size % b->descriptor_size ||
            b->memory_map > UINT64_MAX - b->memory_map_size)
            return BOOT_INFO_MEMORY_MAP;
        if (b->dtb) return BOOT_INFO_DTB;
        if (b->boot_el) return BOOT_INFO_EL;
        if (b->uart_base != 0x3f8) return BOOT_INFO_PLATFORM;
    } else {
        if (b->platform != STYLEOS_PLATFORM_QEMU_VIRT) return BOOT_INFO_PLATFORM;
        if (b->flags != STYLEOS_BOOT_HAS_DTB) return BOOT_INFO_FLAGS;
        if (b->memory_map || b->memory_map_size || b->descriptor_size ||
            b->descriptor_version) return BOOT_INFO_MEMORY_MAP;
        if (!b->dtb || (b->dtb & 7)) return BOOT_INFO_DTB;
        if (b->boot_el != 1) return BOOT_INFO_EL;
        if (b->uart_base != 0x09000000) return BOOT_INFO_PLATFORM;
    }
    return BOOT_INFO_OK;
}

const char *boot_info_error_name(enum boot_info_error error)
{
    switch (error) {
    case BOOT_INFO_OK: return "ok";
    case BOOT_INFO_NULL: return "null";
    case BOOT_INFO_MAGIC: return "magic";
    case BOOT_INFO_VERSION: return "version";
    case BOOT_INFO_SIZE: return "size";
    case BOOT_INFO_ARCH: return "arch";
    case BOOT_INFO_PLATFORM: return "platform";
    case BOOT_INFO_FLAGS: return "flags";
    case BOOT_INFO_MEMORY_MAP: return "memory-map";
    case BOOT_INFO_DTB: return "dtb";
    case BOOT_INFO_EL: return "exception-level";
    case BOOT_INFO_RESERVED: return "reserved";
    }
    return "unknown";
}
