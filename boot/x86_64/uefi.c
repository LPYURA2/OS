#include "exit_boot.h"
#include <styleos/arch.h>
#include <styleos/boot_memory.h>
#if defined(STYLEOS_TEST_DESKTOP_DISPLAY) || defined(STYLEOS_DESKTOP_V0)
#include "desktop_display.h"
#endif

static struct boot_range loaded_image;

static struct boot_info handoff = {
    .magic = STYLEOS_BOOT_MAGIC,
    .version = STYLEOS_BOOT_VERSION,
    .size = sizeof(struct boot_info),
    .arch = STYLEOS_ARCH_X86_64,
    .platform = STYLEOS_PLATFORM_UEFI,
    .uart_base = 0x3f8u,
};

_Noreturn void EFIAPI styleos_enter_kernel(const struct boot_info *);

static void firmware_error(struct efi_system_table *table)
{
    if (table != NULL && table->con_out != NULL &&
        table->con_out->output_string != NULL)
        (void)table->con_out->output_string(
            table->con_out, (CHAR16 *)u"StyleOS boot error; see COM1 diagnostics.\r\n");
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, struct efi_system_table *table)
{
    EFI_STATUS status;
    arch_console_init();
    arch_puts("STYLEOS x86_64 UEFI entry\n");

    if (table == NULL || table->hdr.signature != EFI_SYSTEM_TABLE_SIGNATURE ||
        table->hdr.header_size < sizeof(struct efi_system_table)) {
        arch_puts("STYLEOS BOOT_ERROR invalid UEFI system table\n");
        return EFI_INVALID_PARAMETER;
    }
    if (table->boot_services == NULL ||
        table->boot_services->hdr.signature != EFI_BOOT_SERVICES_SIGNATURE ||
        table->boot_services->hdr.header_size < sizeof(struct efi_boot_services)) {
        arch_puts("STYLEOS BOOT_ERROR invalid UEFI boot services\n");
        firmware_error(table);
        return EFI_INVALID_PARAMETER;
    }
    struct efi_guid loaded_image_guid = {
        0x5b1b31a1, 0x9562, 0x11d2,
        {0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
    };
    void *interface = NULL;
    if (!table->boot_services->handle_protocol ||
        table->boot_services->handle_protocol(image, &loaded_image_guid,
                                               &interface) != EFI_SUCCESS ||
        !interface) {
        arch_puts("STYLEOS BOOT_ERROR Loaded Image unavailable\n");
        return EFI_UNSUPPORTED;
    }
    const struct efi_loaded_image *loaded = interface;
    if (!loaded->image_base ||
        bm_extent((uintptr_t)loaded->image_base, loaded->image_size,
                  &loaded_image)) {
        arch_puts("STYLEOS BOOT_ERROR invalid Loaded Image extent\n");
        return EFI_INVALID_PARAMETER;
    }
#if defined(STYLEOS_TEST_DESKTOP_DISPLAY) || defined(STYLEOS_DESKTOP_V0)
    desktop_capture_uefi_mode(table->boot_services);
#endif
    arch_puts("STYLEOS x86_64 acquiring memory map\n");
    status = styleos_exit_boot_services(table->boot_services, image, &handoff);
    if (status != EFI_SUCCESS) {
        arch_puts("STYLEOS BOOT_ERROR GetMemoryMap/ExitBootServices failed\n");
        arch_puts("STYLEOS UEFI status 0x");
        for (int shift = 60; shift >= 0; shift -= 4)
            arch_putc("0123456789abcdef"[(status >> shift) & 15u]);
        arch_puts("\n");
        /* ExitBootServices can partially shut firmware down even on failure.
         * Do not call its console protocols or resume the firmware after an
         * unsuccessful attempt. A failed handoff is terminal in this prototype. */
        arch_halt();
    }

    /* No UEFI interface may be used below this point. */
    __asm__ volatile ("cli" ::: "memory");
    arch_puts("STYLEOS x86_64 ExitBootServices OK\n");
#ifdef STYLEOS_TEST_INVALID_BOOT_INFO
    handoff.magic = 0;
#endif
    styleos_enter_kernel(&handoff);
}


extern const unsigned char styleos_stack_bottom[], styleos_stack_top[];

enum boot_memory_error boot_memory_collect(const struct boot_info *info,
                                            struct boot_memory_map *map,
                                            struct boot_memory_live *live)
{
    live->image = loaded_image;
    live->stack = (struct boot_range){(uintptr_t)styleos_stack_bottom,
                                     (uintptr_t)styleos_stack_top};
    enum boot_memory_error e = bm_extent((uintptr_t)info, sizeof(*info),
                                          &live->handoff);
    if (e) return e;
    e = bm_extent(info->memory_map, info->memory_map_size, &live->source);
    if (e) return e;
    size_t size = info->memory_map_size;
#ifdef STYLEOS_TEST_INVALID_MEMORY_MAP
    --size; /* Drop a byte of the last descriptor; leave boot_info intact. */
#endif
    return bm_uefi_parse(map, (const void *)(uintptr_t)info->memory_map,
                         size, info->descriptor_size, info->descriptor_version);
}
