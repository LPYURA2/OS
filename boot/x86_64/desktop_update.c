#include "efi.h"
#include "styleos_update_key.h"
#include <styleos/arch.h>
#include <styleos/system_update.h>

#define STRINGIFY_VALUE(x) #x
#define STRINGIFY(x) STRINGIFY_VALUE(x)

typedef EFI_STATUS (EFIAPI *load_fn)(uint8_t, EFI_HANDLE, void *, void *, UINTN, EFI_HANDLE *);
typedef EFI_STATUS (EFIAPI *start_fn)(EFI_HANDLE, UINTN *, CHAR16 **);
typedef EFI_STATUS (EFIAPI *unload_fn)(EFI_HANDLE);
_Static_assert(offsetof(struct efi_boot_services, load_image) == 200 &&
               offsetof(struct efi_boot_services, start_image) == 208 &&
               offsetof(struct efi_boot_services, unload_image) == 224, "UEFI image services");

struct update_boot { uint64_t magic; uint32_t slot, build; };
#define BOOT_TOKEN UINT64_C(0x3130544f4f425553)
static struct update_boot child_options;
static struct upd_disk disk;
static uint8_t buffer[UP_BLOB_MAX];
static unsigned boot_slot;

static void note(const char *message)
{
    arch_puts("STYLEOS DESKTOP UPDATE ");
    arch_puts(message);
    arch_putc('\n');
}

uint8_t *desk_update_buffer(void) { return buffer; }

bool desk_update_probe(void) { return disk.valid; }
bool desk_update_pending(void) { return disk.valid && disk.state.pending_slot; }
const char *desk_update_status(void)
{
    if (!disk.valid) return "Системный диск обновлений недоступен";
    if (disk.state.pending_slot) return "Пакет готов. Перезагрузите систему";
    return "Сборка " STRINGIFY(STYLEOS_UPDATE_BUILD) " / Откройте пакет .supdate";
}

enum upd_result desk_update_stage(size_t bytes)
{
    struct upd_package rec;
    enum upd_result r = upd_stage(&disk, buffer, bytes, STYLEOS_UPDATE_BUILD, styleos_update_key, &rec);
    switch (r) {
    case UP_OK: note("STAGED"); break;
    case UP_AUTH: note("REJECT SIGNATURE"); break;
    case UP_OLD: note("REJECT VERSION"); break;
    case UP_BUSY: note("REJECT PENDING"); break;
    case UP_FORMAT: note("REJECT FORMAT"); break;
    case UP_DISK: note("REJECT DISK"); break;
    case UP_IO: note("REJECT IO"); break;
    }
    return r;
}

static bool read_slot(unsigned slot, uint32_t expected, struct upd_package *rec)
{
    return upd_load(&disk, slot, buffer, styleos_update_key, rec) == UP_OK && rec->build == expected;
}

static void launch(EFI_HANDLE parent, struct efi_boot_services *bs, struct efi_guid *guid,
                   unsigned slot, const struct upd_package *rec)
{
    load_fn load = (load_fn)bs->load_image;
    start_fn start = (start_fn)bs->start_image;
    unload_fn unload = (unload_fn)bs->unload_image;
    if (!load || !start || !unload) { note("FIRMWARE API UNAVAILABLE"); return; }
    EFI_HANDLE child = 0;
    EFI_STATUS status = load(0, parent, 0, buffer + UP_SECTOR, rec->bytes, &child);
    if (status != EFI_SUCCESS || !child) {
        if (child) (void)unload(child);
        note("LOAD FAILED");
        return;
    }
    void *interface = 0;
    if (bs->handle_protocol(child, guid, &interface) != EFI_SUCCESS || !interface) {
        (void)unload(child);
        note("LOADED IMAGE PROTOCOL UNAVAILABLE");
        return;
    }
    struct efi_loaded_image *loaded = interface;
    child_options = (struct update_boot){BOOT_TOKEN, slot, rec->build};
    loaded->load_options_size = sizeof(child_options);
    loaded->load_options = &child_options;
    (void)start(child, 0, 0);
    /* An EFI application which exits is unloaded by the firmware. */
    note("CANDIDATE RETURNED");
}

EFI_STATUS desk_update_boot(EFI_HANDLE image, struct efi_boot_services *bs,
                            const struct efi_loaded_image *loaded, struct efi_guid *guid)
{
    struct upd_io io;
    if (desk_update_io(&io)) (void)upd_open(&disk, &io);
    if (loaded->load_options_size == sizeof(struct update_boot) && loaded->load_options) {
        const unsigned char *raw = loaded->load_options;
        struct update_boot options;
        unsigned char *dest = (unsigned char *)&options;
        for (unsigned i = 0; i < sizeof(options); ++i) dest[i] = raw[i];
        if (options.magic == BOOT_TOKEN) {
            if (!loaded->parent_handle || !disk.valid || options.slot < 1 || options.slot > 2 ||
                options.build != STYLEOS_UPDATE_BUILD ||
                !((disk.state.tried && disk.state.pending_slot == options.slot &&
                   disk.state.pending_build == options.build) ||
                  (disk.state.active_slot == options.slot && disk.state.active_build == options.build)))
                return EFI_UNSUPPORTED;
            boot_slot = options.slot;
            note("RUN " STRINGIFY(STYLEOS_UPDATE_BUILD));
#ifdef STYLEOS_TEST_UPDATE_FAIL
            note("TEST STUCK");
            for (;;) __asm__ volatile ("pause");
#endif
            return EFI_SUCCESS;
        }
    }
#ifdef STYLEOS_TEST_UPDATE_FAIL
    return EFI_UNSUPPORTED;
#endif
    if (disk.valid && disk.state.pending_slot) {
        unsigned slot = disk.state.pending_slot;
        struct upd_package rec;
        if (!disk.state.tried && read_slot(slot, disk.state.pending_build, &rec) && upd_trial(&disk)) {
            note("TRIAL");
            launch(image, bs, guid, slot, &rec);
        }
        if (upd_rollback(&disk)) note("ROLLBACK");
        else note("ROLLBACK JOURNAL FAILED");
    }
    if (disk.valid && disk.state.active_slot) {
        struct upd_package rec;
        if (read_slot(disk.state.active_slot, disk.state.active_build, &rec))
            launch(image, bs, guid, disk.state.active_slot, &rec);
        else note("ACTIVE SLOT INVALID");
    }
    note("RUN " STRINGIFY(STYLEOS_UPDATE_BUILD));
    return EFI_SUCCESS;
}

void desk_update_mark_ready(void)
{
    if (boot_slot && disk.valid && disk.state.tried && disk.state.pending_slot == boot_slot) {
        if (upd_accept(&disk, boot_slot, STYLEOS_UPDATE_BUILD)) note("ACCEPTED");
        else note("ACCEPT JOURNAL FAILED");
    }
}
