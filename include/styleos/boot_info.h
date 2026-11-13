#ifndef STYLEOS_BOOT_INFO_H
#define STYLEOS_BOOT_INFO_H

#include <stdint.h>
#include <stddef.h>

#define STYLEOS_BOOT_MAGIC UINT64_C(0x5354594c454f5342)
#define STYLEOS_BOOT_VERSION 1u
#define STYLEOS_ARCH_X86_64 1u
#define STYLEOS_ARCH_AARCH64 2u
#define STYLEOS_PLATFORM_UEFI 1u
#define STYLEOS_PLATFORM_QEMU_VIRT 2u
#define STYLEOS_BOOT_EXITED_SERVICES UINT64_C(1)
#define STYLEOS_BOOT_HAS_DTB UINT64_C(2)

/* Fixed-width v1 physical-address contract, 96 bytes, naturally aligned to 8.
 * No C pointers, firmware callbacks or allocator ownership cross this boundary.
 * Buffers/image/stack must remain reserved until a future memory manager adopts
 * them. A version/layout change requires a new version and owner review. */
struct boot_info {
    uint64_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t arch;
    uint32_t platform;
    uint64_t flags;
    uint64_t memory_map;
    uint64_t memory_map_size;
    uint32_t descriptor_size;
    uint32_t descriptor_version;
    uint64_t dtb;
    uint64_t uart_base;
    uint32_t boot_el;
    uint32_t reserved0;
    uint64_t reserved[2];
};

_Static_assert(sizeof(struct boot_info) == 96, "boot_info v1 size");
_Static_assert(offsetof(struct boot_info, memory_map) == 32, "memory map offset");
_Static_assert(offsetof(struct boot_info, dtb) == 56, "DTB offset");
_Static_assert(offsetof(struct boot_info, boot_el) == 72, "EL offset");

enum boot_info_error {
    BOOT_INFO_OK, BOOT_INFO_NULL, BOOT_INFO_MAGIC, BOOT_INFO_VERSION,
    BOOT_INFO_SIZE, BOOT_INFO_ARCH, BOOT_INFO_PLATFORM, BOOT_INFO_FLAGS,
    BOOT_INFO_MEMORY_MAP, BOOT_INFO_DTB, BOOT_INFO_EL, BOOT_INFO_RESERVED
};

enum boot_info_error boot_info_validate(const struct boot_info *, uint32_t arch);
const char *boot_info_error_name(enum boot_info_error);
_Noreturn void kernel_main(const struct boot_info *);

#endif
