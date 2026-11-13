#include "exit_boot.h"
#include <styleos/arch.h>

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
