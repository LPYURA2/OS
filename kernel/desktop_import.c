#include <styleos/arch.h>
#include <styleos/desktop_import.h>
#include <styleos/desktop_stylefs.h>
#include <styleos/system_update.h>

enum { HEADER = 12, CHUNK = 12, ACK = 6, NAK = 21, MINIMUM = UP_SECTOR + 128 };
static struct import_session {
    bool ready, probed, active;
    unsigned parent, received, size, frame;
    uint32_t generation;
    uint64_t last;
    uint8_t header[HEADER];
} transfer;

bool desk_import_available(void)
{
    if (!transfer.probed) {
        transfer.probed = true;
        transfer.ready = desk_com2_init();
    }
    return transfer.ready;
}

bool desk_import_active(void) { return transfer.active; }

static void status(struct desk_fs *b, const char *message)
{
    b->status = message;
    ++b->revision;
}

static bool finish(struct desk_fs *b, bool accepted, const char *message, const char *log)
{
    transfer.active = false;
    (void)desk_com2_write(accepted ? ACK : NAK);
    status(b, message);
    arch_puts("STYLEOS DESKTOP IMPORT ");
    arch_puts(log);
    arch_putc('\n');
    return true;
}

void desk_import_begin(struct desk_fs *b)
{
    if (transfer.active) return;
    if (!desk_import_available()) { status(b, "Канал с ПК недоступен. Включите COM2 в ВМ"); return; }
    if (!b->disk.mounted || !b->update_view || b->computer ||
        b->folder == SFS_ROOT || sfs_file_limit(&b->disk) < MINIMUM) {
        status(b, "Импорт доступен в папке обновлений");
        return;
    }
    for (unsigned i = 0; i < 64 && desk_com2_read() >= 0; ++i) {}
    transfer.active = true;
    transfer.parent = b->folder;
    transfer.generation = b->disk.generation;
    transfer.received = transfer.size = transfer.frame = 0;
    transfer.last = desk_pointer_clock();
    status(b, "Жду файл с ПК. Esc - отменить");
    arch_puts("STYLEOS DESKTOP IMPORT ARMED\n");
}

void desk_import_cancel(struct desk_fs *b)
{
    if (transfer.active) (void)finish(b, false, "Импорт отменён", "CANCELLED");
}

static void package_name(char name[25], uint32_t build)
{
    char rev[10];
    const char prefix[] = "Build-";
    const char suffix[] = ".supdate";
    unsigned n = 0, at = 0;
    do { rev[n++] = (char)('0' + build % 10); build /= 10; } while (build);
    for (unsigned i = 0; i < sizeof(prefix) - 1; ++i) name[at++] = prefix[i];
    while (n) name[at++] = rev[--n];
    for (unsigned i = 0; i < sizeof(suffix); ++i) name[at++] = suffix[i];
}

static bool publish(struct desk_fs *b)
{
    struct upd_package package;
    enum upd_result r = upd_check(desk_update_buffer(), transfer.size, styleos_update_key, &package);
    if (r != UP_OK) return finish(b, false, "Пакет с ПК не прошёл проверку подписи или формата", "INVALID");
    if (!b->disk.mounted || !b->update_view || transfer.parent != b->folder ||
        b->disk.generation != transfer.generation)
        return finish(b, false, "Папка или диск изменились", "DISK CHANGED");

    char name[25];
    package_name(name, package.build);
    unsigned id = 0;
    enum sfs_result saved = sfs_create(&b->disk, &b->work, transfer.parent, name,
                                        desk_update_buffer(), transfer.size, &id);
    if (saved == SFS_EXISTS) return finish(b, false, "Такой пакет уже есть. Файл не изменён", "EXISTS");
    if (saved != SFS_OK) return finish(b, false, "Не удалось сохранить пакет на диске", "WRITE FAILED");
    /* Clear an active filter so the imported file is visible in the folder. */
    b->query_length = b->first = 0;
    b->query[0] = 0;
    b->searching = false;
    b->selected = (int)id;
    return finish(b, true, "Пакет сохранён. Откройте его для установки", "SAVED");
}

bool desk_import_poll(struct desk_fs *b)
{
    if (!transfer.active) return false;
    uint64_t now = desk_pointer_clock();
    if (desk_pointer_idle(transfer.last, now, transfer.size ? 15 : 120))
        return finish(b, false, "Время ожидания истекло. Можно повторить", "TIMEOUT");
    for (unsigned i = 0; i < 64; ++i) {
        int got = desk_com2_read();
        if (got == -1) return false;
        if (got < -1) return finish(b, false, "Ошибка канала. Попробуйте отправить снова", "UART ERROR");
        uint8_t byte = (uint8_t)got;
        transfer.last = now;
        if (!transfer.size) {
            transfer.header[transfer.received++] = byte;
            if (transfer.received == HEADER) {
                const char magic[] = "STYHOST1";
                for (unsigned j = 0; j < 8; ++j)
                    if (transfer.header[j] != (uint8_t)magic[j])
                        return finish(b, false, "Неверный запрос от ПК", "HEADER");
                uint32_t size = 0;
                for (unsigned j = 0; j < 4; ++j) size |= (uint32_t)transfer.header[8 + j] << (8 * j);
                if (size < MINIMUM || size > UP_BLOB_MAX || size > sfs_file_limit(&b->disk))
                    return finish(b, false, "Недопустимый размер пакета", "SIZE");
                transfer.size = size;
                transfer.received = 0;
                if (!desk_com2_write(ACK)) return finish(b, false, "Ошибка связи с ПК", "UART ERROR");
            }
        } else {
            desk_update_buffer()[transfer.received++] = byte;
            if (transfer.received == transfer.size) return publish(b);
            if (++transfer.frame == CHUNK) {
                transfer.frame = 0;
                if (!desk_com2_write(ACK)) return finish(b, false, "Ошибка связи с ПК", "UART ERROR");
            }
        }
    }
    return false;
}
