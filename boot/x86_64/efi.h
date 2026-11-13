#ifndef STYLEOS_X86_EFI_H
#define STYLEOS_X86_EFI_H

#include <stddef.h>
#include <stdint.h>

/* Only the UEFI interfaces used by this prototype. All offsets are checked;
 * the firmware calls use the Microsoft x64 ABI, including in native tests. */
#if defined(__x86_64__)
#define EFIAPI __attribute__((ms_abi))
#else
#define EFIAPI
#endif
typedef uint64_t EFI_STATUS;
typedef void *EFI_HANDLE;
typedef uint64_t UINTN;
typedef uint16_t CHAR16;
#define EFI_ERROR_BIT UINT64_C(0x8000000000000000)
#define EFI_SUCCESS UINT64_C(0)
#define EFI_INVALID_PARAMETER (EFI_ERROR_BIT | UINT64_C(2))
#define EFI_UNSUPPORTED (EFI_ERROR_BIT | UINT64_C(3))
#define EFI_BUFFER_TOO_SMALL (EFI_ERROR_BIT | UINT64_C(5))
#define EFI_DEVICE_ERROR (EFI_ERROR_BIT | UINT64_C(7))
#define EFI_OUT_OF_RESOURCES (EFI_ERROR_BIT | UINT64_C(9))
#define EFI_ABORTED (EFI_ERROR_BIT | UINT64_C(21))
#define EFI_SYSTEM_TABLE_SIGNATURE UINT64_C(0x5453595320494249)
#define EFI_BOOT_SERVICES_SIGNATURE UINT64_C(0x56524553544f4f42)
#define EFI_LOADER_DATA 2u

struct efi_table_header {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;
};

struct efi_memory_descriptor {
    uint32_t type;
    uint32_t padding;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
};

typedef EFI_STATUS (EFIAPI *efi_get_memory_map)(
    UINTN *, struct efi_memory_descriptor *, UINTN *, UINTN *, uint32_t *);
typedef EFI_STATUS (EFIAPI *efi_allocate_pool)(uint32_t, UINTN, void **);
typedef EFI_STATUS (EFIAPI *efi_free_pool)(void *);
typedef EFI_STATUS (EFIAPI *efi_exit_boot_services)(EFI_HANDLE, UINTN);

struct efi_guid {
    uint32_t a;
    uint16_t b, c;
    uint8_t d[8];
};

struct efi_loaded_image {
    uint32_t revision;
    EFI_HANDLE parent_handle;
    void *system_table;
    EFI_HANDLE device_handle;
    void *file_path;
    void *reserved;
    uint32_t load_options_size;
    void *load_options;
    void *image_base;
    uint64_t image_size;
};

typedef EFI_STATUS (EFIAPI *efi_handle_protocol)(
    EFI_HANDLE, struct efi_guid *, void **);

struct efi_boot_services {
    struct efi_table_header hdr;
    void *raise_tpl;
    void *restore_tpl;
    void *allocate_pages;
    void *free_pages;
    efi_get_memory_map get_memory_map;
    efi_allocate_pool allocate_pool;
    efi_free_pool free_pool;
    void *create_event;
    void *set_timer;
    void *wait_for_event;
    void *signal_event;
    void *close_event;
    void *check_event;
    void *install_protocol_interface;
    void *reinstall_protocol_interface;
    void *uninstall_protocol_interface;
    efi_handle_protocol handle_protocol;
    void *reserved;
    void *register_protocol_notify;
    void *locate_handle;
    void *locate_device_path;
    void *install_configuration_table;
    void *load_image;
    void *start_image;
    void *exit;
    void *unload_image;
    efi_exit_boot_services exit_boot_services;
};

struct efi_simple_text_output {
    void *reset;
    EFI_STATUS (EFIAPI *output_string)(struct efi_simple_text_output *, CHAR16 *);
};

struct efi_system_table {
    struct efi_table_header hdr;
    CHAR16 *firmware_vendor;
    uint32_t firmware_revision;
    uint32_t padding;
    EFI_HANDLE console_in_handle;
    void *con_in;
    EFI_HANDLE console_out_handle;
    struct efi_simple_text_output *con_out;
    EFI_HANDLE standard_error_handle;
    void *std_err;
    void *runtime_services;
    struct efi_boot_services *boot_services;
    UINTN number_of_table_entries;
    void *configuration_table;
};

_Static_assert(sizeof(UINTN) == 8, "UEFI x64 UINTN");
_Static_assert(sizeof(struct efi_table_header) == 24, "UEFI header size");
_Static_assert(sizeof(struct efi_memory_descriptor) == 40, "UEFI descriptor");
_Static_assert(offsetof(struct efi_boot_services, get_memory_map) == 56,
               "UEFI GetMemoryMap offset");
_Static_assert(sizeof(struct efi_guid) == 16, "UEFI GUID size");
_Static_assert(offsetof(struct efi_loaded_image, image_base) == 64 &&
               offsetof(struct efi_loaded_image, image_size) == 72,
               "UEFI Loaded Image fields");
_Static_assert(offsetof(struct efi_boot_services, handle_protocol) == 152,
               "UEFI HandleProtocol offset");
_Static_assert(offsetof(struct efi_boot_services, exit_boot_services) == 232,
               "UEFI ExitBootServices offset");
_Static_assert(offsetof(struct efi_system_table, con_out) == 64,
               "UEFI ConOut offset");
_Static_assert(offsetof(struct efi_system_table, boot_services) == 96,
               "UEFI BootServices offset");
#endif
