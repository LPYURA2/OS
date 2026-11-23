#include "exit_boot.h"
#include <styleos/arch.h>
#include <styleos/boot_memory.h>
#if defined(STYLEOS_TEST_DESKTOP_DISPLAY) || defined(STYLEOS_DESKTOP_V0) || defined(STYLEOS_TEST_HW_PROBE_793) || defined(STYLEOS_TEST_PCI_PROBE_794) || defined(STYLEOS_TEST_PCI_APERTURE_795) || defined(STYLEOS_TEST_PCI_TOPOLOGY_796) || defined(STYLEOS_TEST_PCI_RANGE_799) || defined(STYLEOS_TEST_CACHE_800) || defined(STYLEOS_TEST_CACHE_801) || defined(STYLEOS_TEST_CACHE_803) || defined(STYLEOS_TEST_CACHE_804)
#include "desktop_display.h"
#endif

#if defined(STYLEOS_TEST_DMAR_808) || defined(STYLEOS_TEST_VTD_809)
void dmar808_capture(const struct efi_system_table *);
_Noreturn void dmar808_terminal(const struct boot_info *);
#endif
#ifdef STYLEOS_TEST_ACPICA_CORE_821
void acpi821_terminal(const struct boot_info *);
#endif
#ifdef STYLEOS_TEST_ACPICA_AML_822
void acpi822_terminal(const struct boot_info *);
#endif
#ifdef STYLEOS_TEST_ACPI_NAMESPACE_816
void pci816_capture(const struct efi_system_table *);
_Noreturn void pci816_terminal(const struct boot_info *);
#endif
#ifdef STYLEOS_TEST_ACPI_HOSTBRIDGE_815
void pci815_capture(const struct efi_system_table *);
_Noreturn void pci815_terminal(const struct boot_info *);
#endif
#ifdef STYLEOS_TEST_PCI_ROOTBRIDGE_814
void pci814_capture(const struct efi_system_table *);
_Noreturn void pci814_terminal(const struct boot_info *);
#endif
#ifdef STYLEOS_TEST_PCI_MCFG_813
void pci813_capture(const struct efi_system_table *);
_Noreturn void pci813_terminal(const struct boot_info *);
#endif
#ifdef STYLEOS_TEST_PCI_REQUESTER_812
void pci812_capture(const struct efi_system_table *);
_Noreturn void pci812_terminal(const struct boot_info *);
#endif
#ifdef STYLEOS_TEST_PCI_REQUESTER_811
void pci811_capture(const struct efi_system_table *);
_Noreturn void pci811_terminal(const struct boot_info *);
#endif
#ifdef STYLEOS_TEST_IOMMU_810
void iommu810_capture(const struct efi_system_table *);
_Noreturn void iommu810_terminal(const struct boot_info *);
#endif
#ifdef STYLEOS_TEST_VTD_809
_Noreturn void vtd809_terminal(const struct boot_info *);
#endif
#ifdef STYLEOS_TEST_IRQ_DELIVERY_807
void acpi806_capture(const struct efi_system_table *);
_Noreturn void irq807_terminal(const struct boot_info *);
#endif
#ifdef STYLEOS_TEST_ACPI_MADT_806
void acpi806_capture(const struct efi_system_table *);
_Noreturn void acpi806_terminal(const struct boot_info *);
#endif
#ifdef STYLEOS_TEST_IRQ_APIC_805
_Noreturn void irq805_terminal(const struct boot_info *);
#endif
#ifdef STYLEOS_TEST_HW_PROBE_793
#include <styleos/desktop_display.h>
_Noreturn void hw793_terminal(const struct boot_info *, const struct desktop_surface *);
#endif
#ifdef STYLEOS_TEST_PCI_PROBE_794
#include <styleos/desktop_display.h>
_Noreturn void pci794_terminal(const struct boot_info *, const struct desktop_surface *);
#endif

#ifdef STYLEOS_TEST_CACHE_803
#include <styleos/desktop_display.h>
_Noreturn void cache803_terminal(const struct boot_info *,const struct desktop_surface *);
#endif
#ifdef STYLEOS_TEST_CACHE_804
#include <styleos/desktop_display.h>
_Noreturn void cache804_terminal(const struct boot_info *,const struct desktop_surface *);
#endif
#ifdef STYLEOS_TEST_CACHE_801
#include <styleos/desktop_display.h>
_Noreturn void cache801_terminal(const struct boot_info *,const struct desktop_surface *);
#endif
#ifdef STYLEOS_TEST_CACHE_800
#include <styleos/desktop_display.h>
_Noreturn void cache800_terminal(const struct boot_info *,const struct desktop_surface *);
#endif
#ifdef STYLEOS_TEST_PCI_RANGE_799
#include <styleos/desktop_display.h>
_Noreturn void pci799_terminal(const struct boot_info *, const struct desktop_surface *);
#endif
#ifdef STYLEOS_TEST_PCI_TOPOLOGY_796
#include <styleos/desktop_display.h>
_Noreturn void pci796_terminal(const struct boot_info *, const struct desktop_surface *);
#endif

#ifdef STYLEOS_TEST_PCI_APERTURE_795
#include <styleos/desktop_display.h>
_Noreturn void pci795_terminal(const struct boot_info *, const struct desktop_surface *);
#endif

#ifdef STYLEOS_DESKTOP_UPDATE
EFI_STATUS desk_update_boot(EFI_HANDLE, struct efi_boot_services *,
                            const struct efi_loaded_image *, struct efi_guid *);
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
#ifdef STYLEOS_DESKTOP_UPDATE
    status = desk_update_boot(image, table->boot_services, loaded, &loaded_image_guid);
    if (status != EFI_SUCCESS) return status;
#endif
#if defined(STYLEOS_TEST_DESKTOP_DISPLAY) || defined(STYLEOS_DESKTOP_V0) || defined(STYLEOS_TEST_HW_PROBE_793) || defined(STYLEOS_TEST_PCI_PROBE_794) || defined(STYLEOS_TEST_PCI_APERTURE_795) || defined(STYLEOS_TEST_PCI_TOPOLOGY_796) || defined(STYLEOS_TEST_PCI_RANGE_799) || defined(STYLEOS_TEST_CACHE_800) || defined(STYLEOS_TEST_CACHE_801) || defined(STYLEOS_TEST_CACHE_803) || defined(STYLEOS_TEST_CACHE_804)
    desktop_capture_uefi_mode(table->boot_services);
#endif
#if defined(STYLEOS_TEST_ACPI_MADT_806) || defined(STYLEOS_TEST_IRQ_DELIVERY_807)
    acpi806_capture(table);
#endif
#if defined(STYLEOS_TEST_DMAR_808) || defined(STYLEOS_TEST_VTD_809)
    dmar808_capture(table);
#endif
#ifdef STYLEOS_TEST_IOMMU_810
    iommu810_capture(table);
#endif
#ifdef STYLEOS_TEST_ACPI_NAMESPACE_816
    pci816_capture(table);
#endif
#ifdef STYLEOS_TEST_ACPI_HOSTBRIDGE_815
    pci815_capture(table);
#endif
#ifdef STYLEOS_TEST_PCI_ROOTBRIDGE_814
    pci814_capture(table);
#endif
#ifdef STYLEOS_TEST_PCI_MCFG_813
    pci813_capture(table);
#endif
#ifdef STYLEOS_TEST_PCI_REQUESTER_812
    pci812_capture(table);
#endif
#ifdef STYLEOS_TEST_PCI_REQUESTER_811
    pci811_capture(table);
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
#ifdef STYLEOS_TEST_IOMMU_810
    iommu810_terminal(&handoff);
#endif
#ifdef STYLEOS_TEST_ACPICA_CORE_821
    acpi821_terminal(&handoff);
#endif
#ifdef STYLEOS_TEST_ACPICA_AML_822
    acpi822_terminal(&handoff);
#endif
#ifdef STYLEOS_TEST_ACPI_NAMESPACE_816
    pci816_terminal(&handoff);
#endif
#ifdef STYLEOS_TEST_ACPI_HOSTBRIDGE_815
    pci815_terminal(&handoff);
#endif
#ifdef STYLEOS_TEST_PCI_ROOTBRIDGE_814
    pci814_terminal(&handoff);
#endif
#ifdef STYLEOS_TEST_PCI_MCFG_813
    pci813_terminal(&handoff);
#endif
#ifdef STYLEOS_TEST_PCI_REQUESTER_812
    pci812_terminal(&handoff);
#endif
#ifdef STYLEOS_TEST_PCI_REQUESTER_811
    pci811_terminal(&handoff);
#endif
#ifdef STYLEOS_TEST_VTD_809
    vtd809_terminal(&handoff);
#endif
#ifdef STYLEOS_TEST_DMAR_808
    dmar808_terminal(&handoff);
#endif
#ifdef STYLEOS_TEST_IRQ_DELIVERY_807
    irq807_terminal(&handoff);
#endif
#ifdef STYLEOS_TEST_ACPI_MADT_806
    acpi806_terminal(&handoff);
#endif
#ifdef STYLEOS_TEST_IRQ_APIC_805
    irq805_terminal(&handoff);
#endif
#ifdef STYLEOS_TEST_HW_PROBE_793
    hw793_terminal(&handoff, desktop_platform_surface());
#endif
#ifdef STYLEOS_TEST_PCI_PROBE_794
    pci794_terminal(&handoff, desktop_platform_surface());
#endif
#ifdef STYLEOS_TEST_CACHE_804
    cache804_terminal(&handoff,desktop_platform_surface());
#endif
#ifdef STYLEOS_TEST_CACHE_803
    cache803_terminal(&handoff,desktop_platform_surface());
#endif
#ifdef STYLEOS_TEST_CACHE_801
    cache801_terminal(&handoff,desktop_platform_surface());
#endif
#ifdef STYLEOS_TEST_CACHE_800
    cache800_terminal(&handoff, desktop_platform_surface());
#endif
#ifdef STYLEOS_TEST_PCI_RANGE_799
    pci799_terminal(&handoff, desktop_platform_surface());
#endif
#ifdef STYLEOS_TEST_PCI_TOPOLOGY_796
    pci796_terminal(&handoff, desktop_platform_surface());
#endif
#ifdef STYLEOS_TEST_PCI_APERTURE_795
    pci795_terminal(&handoff, desktop_platform_surface());
#endif
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
