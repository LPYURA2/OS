#include <styleos/desktop_stylefs.h>
#include <styleos/desktop_extra_volume.h>
#include <styleos/arch.h>
#ifdef STYLEOS_DESKTOP_UPDATE
#include <styleos/system_update.h>
#include <styleos/desktop_import.h>
#endif

enum { TEXT = 0xf3f0e9, DIM = 0xaeb8c7, AMBER = 0xe6ad63, BUTTON = 0x253143,
       LINE = 0x49596f, SELECT = 0x76572f, DISABLED = 0x748297,
       FILE_TOP = 154, ROW_H = 31, FOOT = 43, SIDEBAR = 222,
       SIDE_ROWS = 122, COMPUTER_ROW = 350,
       MAKE_DIR = 1, RENAME = 2, COMPUTER = SFS_ENTRIES,
       SYSTEM_ROW = 384, DATA_ROW = 415, UPDATE_ROW = 308, GRID_H = 78 };
static const char *const places[] = {"Загрузки", "Документы", "Изображения", "Музыка", "Видео", "Проекты"};

static int sidebar(struct desk_box a) { return a.w / 3 < SIDEBAR ? a.w / 3 : SIDEBAR; }

static bool read_sector(void *context, uint32_t lba, uint8_t out[512])
{ (void)context; return desk_ata_read(lba, out); }
static bool write_sector(void *context, uint32_t lba, const uint8_t in[512])
{ (void)context; return desk_ata_write(lba, in); }
static bool flush_disk(void *context) { (void)context; return desk_ata_flush(); }

void desk_fs_mount(struct desk_fs *b, uint32_t sectors)
{
    b->mode = sfs_capacity_valid(sectors);
    b->selected = b->clip = b->delete_id = -1;
#ifdef STYLEOS_DESKTOP_UPDATE
    b->update_id = -1;
#endif
    b->history_count = 1;
    b->status = "F2 Имя   Ctrl+C/X/V   Del Удалить";
    if (!b->mode) return;
    /* Preserve the original secondary-master whole-disk mapping. The explicit
     * map allows the installer to supply an offset for Data 1 later without
     * teaching StyleFS about physical IDE ports. */
    const struct desk_physical_disk physical = {
        0, sectors, read_sector, write_sector, flush_disk
    };
#if defined(STYLEOS_STYLEFS_MOUNT_RO) && !defined(STYLEOS_STYLEFS_MOUNT_RW)
    const bool writable = false;
#else
    const bool writable = true;
#endif
    if (!desk_volume_map_init(&b->mapping, &physical, 0, sectors, writable)) {
        b->status = "Том StyleFS недоступен. Диск не изменён";
        return;
    }
    enum sfs_result r = sfs_mount(&b->disk, &b->work, &b->mapping.io);
#ifdef STYLEOS_STYLEFS_DESKTOP_SCOPE
    if (r == SFS_OK) {
        int users=sfs_find(&b->disk,SFS_ROOT,"Users");
        int profile=users<0?-1:sfs_find(&b->disk,(unsigned)users,"TestProfile");
        if(profile<0||!sfs_scope_bind(&b->disk,(unsigned)profile)){
            b->disk.mounted=false;r=SFS_DENIED;
        } else arch_puts("STYLEOS SFS09 SCOPE BOUND\n");
    }
#endif
    if (r == SFS_OK) {
        arch_puts("STYLEOS DESKTOP STYLEFS MOUNT\n");
        if (b->disk.format == 2) arch_puts("STYLEOS DESKTOP STYLEFS V2\n");
#ifdef STYLEOS_STYLEFS_MOUNT_RO
#ifndef STYLEOS_STYLEFS_MOUNT_RW
        b->status = "Том доступен только для чтения";
#endif
        arch_puts("STYLEOS SFS06 ROOT BEGIN\n");
        for (unsigned i=1;i<SFS_ENTRIES;++i) {
            const struct sfs_entry *e=sfs_entry_at(&b->disk,i);
            if(e&&e->parent==SFS_ROOT&&e->type==SFS_DIRECTORY){
                arch_puts("STYLEOS SFS06 DIRECTORY ");arch_puts(e->name);arch_putc('\n');
            }
        }
        arch_puts("STYLEOS SFS06 ROOT END\n");
#endif
    }
    else {
        arch_puts("STYLEOS DESKTOP STYLEFS REFUSED\n");
        b->status = "Том StyleFS недоступен. Диск не изменён";
    }
    desk_extra_volume_mount();
}

static int find_folder(const struct desk_fs *b, const char *name);

static void show(struct desk_fs *b, unsigned id)
{
    if (id != COMPUTER && b->disk.scope_root && !sfs_scope_visible(&b->disk,id)) return;
    b->computer = id == COMPUTER;
    if (!b->computer) b->folder = id;
    b->selected = -1;
    if (b->delete_id >= 0) b->status = "Удаление отменено";
    b->delete_id = -1;
#ifdef STYLEOS_DESKTOP_UPDATE
    b->update_id = -1;
    desk_import_cancel(b);
#endif
    b->first = b->name_edit = b->query_length = 0;
    b->query[0] = 0;
    b->searching = false;
#ifdef STYLEOS_DESKTOP_UPDATE
    b->update_view = !b->computer && b->disk.mounted &&
                     (int)id == find_folder(b, "Обновления");
#else
    b->update_view = false;
#endif
    if (b->disk.mounted) {
        b->status = "";
#ifdef STYLEOS_DESKTOP_UPDATE
        if (b->update_view) b->status = desk_update_status();
#endif
    }
    ++b->revision;
}

static void browse(struct desk_fs *b, unsigned id)
{
    if (id != COMPUTER && b->disk.scope_root && !sfs_scope_visible(&b->disk,id)) return;
    if (b->history[b->history_at] == id) { show(b, id); return; }
    if (b->history_at + 1 == SFS_ENTRIES) {
        for (unsigned i = 1; i < SFS_ENTRIES; ++i) b->history[i - 1] = b->history[i];
        --b->history_at;
    }
    b->history[++b->history_at] = id;
    b->history_count = b->history_at + 1;
    show(b, id);
}

static void history_step(struct desk_fs *b, bool forward)
{
    if (forward && b->history_at + 1 < b->history_count) ++b->history_at;
    else if (!forward && b->history_at > 0) --b->history_at;
    else return;
    show(b, b->history[b->history_at]);
}

static int find_folder(const struct desk_fs *b, const char *name)
{
    int id = sfs_find(&b->disk, SFS_ROOT, name);
    const struct sfs_entry *e = id < 0 ? 0 : sfs_entry_at(&b->disk, (unsigned)id);
    return e && e->type == SFS_DIRECTORY ? id : -1;
}

void desk_fs_shortcut(struct desk_fs *b, unsigned target)
{
    if (target == DESK_ROOT) browse(b, SFS_ROOT);
    else {
        int id = find_folder(b, target == DESK_PROJECTS ? "Проекты" : "Документы");
        if (id >= 0) browse(b, (unsigned)id);
        else { browse(b, SFS_ROOT); if (b->disk.mounted) b->status = "Папка не найдена"; }
    }
}

static unsigned str_length(const char *s)
{
    unsigned n = 0;
    while (s[n]) ++n;
    return n;
}

static uint32_t lower(uint32_t cp)
{
    if ((cp >= 'A' && cp <= 'Z') || (cp >= 0x410 && cp <= 0x42f)) return cp + 32;
    return cp == 0x401 ? 0x451 : cp;
}

static bool matches(const char *name, const struct desk_fs *b)
{
    if (!b->query_length) return true;
    unsigned length = str_length(name);
    for (size_t start = 0, step; start < length; start += step) {
        (void)desk_utf8(name + start, length - start, &step);
        size_t i = start, j = 0, a, q;
        while (j < b->query_length && i < length) {
            uint32_t x = desk_utf8(name + i, length - i, &a);
            uint32_t y = desk_utf8(b->query + j, b->query_length - j, &q);
            if (lower(x) != lower(y)) break;
            i += a;
            j += q;
        }
        if (j == b->query_length) return true;
    }
    return false;
}

static bool search_key(struct desk_fs *b, unsigned key, uint32_t cp)
{
    if (key == 0x1c || key == 0x11c) b->searching = false;
    else if (key == 1) {
        b->query_length = 0;
        b->query[0] = 0;
        b->searching = false;
    } else if (key == 0x0e) {
        if (b->query_length) {
            while (--b->query_length && ((uint8_t)b->query[b->query_length] & 0xc0) == 0x80) {}
            b->query[b->query_length] = 0;
        }
    } else if (cp >= 32 && cp < 0x800 && b->query_length + (cp < 128 ? 1u : 2u) < sizeof(b->query)) {
        if (cp >= 128) b->query[b->query_length++] = (char)(0xc0 | (cp >> 6));
        b->query[b->query_length++] = (char)(cp < 128 ? cp : 0x80 | (cp & 63));
        b->query[b->query_length] = 0;
    } else return false;
    b->first = 0;
    b->selected = -1;
    b->status = b->searching ? "Введите имя файла. Enter / Esc" : "";
    ++b->revision;
    return true;
}

static unsigned rows(const struct desk_fs *b, int out[SFS_ENTRIES])
{
    if (!b->disk.mounted || b->computer) return 0;
    unsigned n = 0;
    for (unsigned type = SFS_DIRECTORY; type <= SFS_FILE; ++type)
        for (unsigned id = 1; id < SFS_ENTRIES; ++id) {
            const struct sfs_entry *e = sfs_entry_at(&b->disk, id);
            if (e && sfs_scope_visible(&b->disk,id) && e->parent == b->folder && e->type == type && matches(e->name, b)) out[n++] = (int)id;
        }
    return n;
}

static int grid_columns(struct desk_box area)
{
    int n = (area.w - sidebar(area) - 25) / 124;
    return n > 1 ? n : 1;
}

static int grid_width(struct desk_box area)
{ return (area.w - sidebar(area) - 25) / grid_columns(area); }

static unsigned visible_rows(const struct desk_fs *b, struct desk_box area)
{
    if (area.h <= FILE_TOP + FOOT) return 0;
    unsigned h = (unsigned)(area.h - FILE_TOP - FOOT);
    return b->grid ? h / GRID_H * (unsigned)grid_columns(area) : h / ROW_H;
}

static void change_selection(struct desk_fs *b, struct desk_box area, int delta)
{
    int ids[SFS_ENTRIES], pos = -1;
    unsigned n = rows(b, ids), visible = visible_rows(b, area);
    if (!n || !visible) return;
    for (unsigned i = 0; i < n; ++i) if (ids[i] == b->selected) pos = (int)i;
    pos = pos < 0 ? 0 : pos + delta;
    if (pos < 0) pos = 0;
    if ((unsigned)pos >= n) pos = (int)n - 1;
    b->selected = ids[pos];
    if ((unsigned)pos < b->first) b->first = (unsigned)pos;
    if ((unsigned)pos >= b->first + visible) b->first = (unsigned)pos + 1 - visible;
    if (b->grid) {
        unsigned cols = (unsigned)grid_columns(area);
        b->first = b->first / cols * cols;
        if ((unsigned)pos >= b->first + visible) b->first += cols;
    }
    ++b->revision;
}

static bool txt_name(const char *name)
{
    unsigned n = 0;
    while (name[n]) ++n;
    if (n < 4 || name[n - 4] != '.') return false;
    return (name[n - 3] | 32) == 't' && (name[n - 2] | 32) == 'x' && (name[n - 1] | 32) == 't';
}

#ifdef STYLEOS_DESKTOP_UPDATE
static bool update_name(const char *s)
{
    const char suffix[] = ".supdate";
    unsigned n = 0;
    while (s[n]) ++n;
    if (n <= sizeof(suffix) - 1) return false;
    for (unsigned i = 0; i < sizeof(suffix) - 1; ++i)
        if ((s[n - (sizeof(suffix) - 1) + i] | 32) != suffix[i]) return false;
    return true;
}

static void update_key(struct desk_fs *b, unsigned key)
{
    if (key == 1) {
        b->update_id = -1;
        b->status = "Обновление отменено";
    } else if (key == 0x1c || key == 0x11c) {
        unsigned id = (unsigned)b->update_id;
        b->update_id = -1;
        const struct sfs_entry *e = sfs_entry_at(&b->disk, id);
        if (b->disk.generation != b->update_generation || !e || e->type != SFS_FILE ||
            !update_name(e->name) || e->size > UP_BLOB_MAX) {
            b->status = "Файл изменился. Откройте его снова";
        } else if (sfs_read(&b->disk, id, desk_update_buffer(), UP_BLOB_MAX) != SFS_OK) {
            b->status = "Не удалось прочитать пакет с диска данных";
        } else switch (desk_update_stage(e->size)) {
        case UP_OK: case UP_BUSY: case UP_DISK: b->status = desk_update_status(); break;
        case UP_AUTH: b->status = "Пакет не прошёл проверку подписи"; break;
        case UP_FORMAT: b->status = "Неверный формат пакета обновления"; break;
        case UP_OLD: b->status = "Пакет не новее установленной версии"; break;
        case UP_IO: b->status = "Ошибка записи. Перезапустите ВМ и проверьте статус"; break;
        }
    } else return;
    ++b->revision;
}
#endif

static int open_selected(struct desk_fs *b)
{
    if (b->selected < 0) return DESK_FS_NONE;
    const struct sfs_entry *e = sfs_entry_at(&b->disk, (unsigned)b->selected);
    if (!e || !sfs_scope_visible(&b->disk,(unsigned)b->selected)) return DESK_FS_NONE;
    if (e->type == SFS_DIRECTORY) { browse(b, (unsigned)b->selected); return DESK_FS_NONE; }
#ifdef STYLEOS_DESKTOP_UPDATE
    if (update_name(e->name)) {
        if (!desk_update_probe() || desk_update_pending()) b->status = desk_update_status();
        else if (e->size > UP_BLOB_MAX || e->size < UP_SECTOR) b->status = "Неверный размер пакета обновления";
        else {
            b->update_id = b->selected;
            b->update_generation = b->disk.generation;
            b->status = "Enter - установить, Esc - отменить";
        }
        ++b->revision;
        return DESK_FS_NONE;
    }
#endif
    if (txt_name(e->name) && e->size <= DESK_STYLEFS_TEXT_LIMIT) return b->selected;
    b->status = e->size > DESK_STYLEFS_TEXT_LIMIT && txt_name(e->name) ? "Текст больше 64 КиБ" : "Можно копировать. Нет приложения";
    arch_puts("STYLEOS DESKTOP STYLEFS NO HANDLER\n");
    ++b->revision;
    return DESK_FS_NONE;
}

static void start_name(struct desk_fs *b, unsigned mode)
{
    if (!b->disk.mounted) return;
    const struct sfs_entry *e = mode == RENAME && b->selected >= 0 ? sfs_entry_at(&b->disk, (unsigned)b->selected) : 0;
    if (mode == RENAME && !e) { b->status = "Сначала выберите файл или папку"; ++b->revision; return; }
    b->name_edit = mode;
    b->name_length = 0;
    b->name[0] = 0;
    if (e) {
        for (unsigned i = 0; i < sizeof(b->name); ++i) b->name[i] = e->name[i];
        while (b->name[b->name_length]) ++b->name_length;
    }
    b->status = mode == RENAME ? "Введите имя. Enter / Esc" : "Введите имя папки. Enter / Esc";
    ++b->revision;
}

static const char *error_text(enum sfs_result r)
{
    switch (r) {
    case SFS_DENIED: arch_puts("STYLEOS SFS09 DENIED\n"); return "Доступ запрещён";
    case SFS_NAME: return "Недопустимое имя";
    case SFS_EXISTS: return "Такое имя уже есть";
    case SFS_FULL: return "Нет места для файла или свободных записей";
    case SFS_NOT_EMPTY: return "Сначала освободите папку";
    case SFS_CORRUPT: return "Контрольная сумма не совпала";
    case SFS_IO: case SFS_STATE: return "Ошибка диска. Перезапустите систему";
    default: return "Не удалось выполнить операцию";
    }
}

static void name_key(struct desk_fs *b, unsigned key, uint32_t cp)
{
    if (key == 1) { b->name_edit = 0; b->status = "Отменено"; }
    else if (key == 0x0e && b->name_length) {
        while (--b->name_length && ((uint8_t)b->name[b->name_length] & 0xc0) == 0x80) {}
        b->name[b->name_length] = 0;
    } else if (key == 0x1c || key == 0x11c) {
        unsigned created;
        bool directory = b->name_edit == MAKE_DIR;
        enum sfs_result r = directory ? sfs_mkdir(&b->disk, &b->work, b->folder, b->name, &created) :
            sfs_move(&b->disk, &b->work, (unsigned)b->selected, b->folder, b->name);
        if (r == SFS_OK) {
            b->name_edit = 0;
            b->status = directory ? "Папка создана" : "Имя изменено";
            arch_puts(directory ? "STYLEOS DESKTOP STYLEFS MKDIR\n" : "STYLEOS DESKTOP STYLEFS RENAME\n");
        } else {
            b->status = error_text(r);
            if (!b->disk.mounted) b->name_edit = 0;
        }
    } else if (cp >= 32 && cp < 0x800) {
        unsigned n = cp < 128 ? 1 : 2;
        if (b->name_length + n <= sfs_name_limit(&b->disk)) {
            if (n == 2) b->name[b->name_length++] = (char)(0xc0 | (cp >> 6));
            b->name[b->name_length++] = (char)(n == 1 ? cp : 0x80 | (cp & 63));
            b->name[b->name_length] = 0;
        }
    }
    ++b->revision;
}

static void go_parent(struct desk_fs *b)
{
    const struct sfs_entry *here = sfs_entry_at(&b->disk, b->folder);
    if (!b->computer && here && b->folder) browse(b, here->parent);
}

static void delete_key(struct desk_fs *b, unsigned key, int protected_entry)
{
    if (key == 1) { b->status = "Удаление отменено"; b->delete_id = -1; }
    else if (key == 0x1c || key == 0x11c) {
        int id = b->delete_id;
        b->delete_id = -1;
        if (id == protected_entry || b->disk.generation != b->delete_generation)
            b->status = "Удаление отменено: файл или диск изменился";
        else {
            enum sfs_result r = sfs_remove(&b->disk, &b->work, (unsigned)id);
            b->status = r == SFS_OK ? "Удалено" : error_text(r);
            if (r == SFS_OK) {
                if (b->clip == id) b->clip = -1;
                b->selected = -1;
                b->first = 0;
                b->history_at = 0;
                b->history_count = 1;
                b->history[0] = b->folder;
                arch_puts("STYLEOS DESKTOP STYLEFS REMOVE\n");
            }
        }
    } else return;
    ++b->revision;
}

int desk_fs_key(struct desk_fs *b, struct desk_box area, unsigned key, unsigned mods, uint32_t cp, int protected_entry)
{
    if (!b->disk.mounted) return DESK_FS_NONE;
#ifdef STYLEOS_DESKTOP_UPDATE
    if (b->update_id >= 0) { if (!mods) update_key(b, key); return DESK_FS_NONE; }
    if (desk_import_active()) {
        if (key == 1 && !mods) desk_import_cancel(b);
        return DESK_FS_NONE;
    }
#endif
    if (b->computer) {
        if (!mods && (key == 0x1c || key == 0x11c)) browse(b, SFS_ROOT);
        return DESK_FS_NONE;
    }
    if (b->name_edit) { if (!(mods & (DESK_ALT | DESK_CTRL))) name_key(b, key, cp); return DESK_FS_NONE; }
    if (b->delete_id >= 0) { if (!mods) delete_key(b, key, protected_entry); return DESK_FS_NONE; }
    if ((mods & DESK_CTRL) && key == 0x21) {
        b->searching = true;
        b->status = "Введите имя файла. Enter / Esc";
        ++b->revision;
        return DESK_FS_NONE;
    }
    if (b->searching) {
        if (!(mods & (DESK_CTRL | DESK_ALT))) (void)search_key(b, key, cp);
        return DESK_FS_NONE;
    }
    if (key == 1 && b->query_length && !mods) { (void)search_key(b, key, 0); return DESK_FS_NONE; }
    if (mods & DESK_ALT) return DESK_FS_NONE;
    if (mods & DESK_CTRL) {
        if (key == 0x31) start_name(b, MAKE_DIR);
        if ((key == 0x2e || key == 0x2d) && b->selected >= 0) {
            const struct sfs_entry *e = sfs_entry_at(&b->disk, (unsigned)b->selected);
            b->clip = -1;
            if (e && (key == 0x2d || e->type == SFS_FILE)) {
                b->clip = b->selected;
                b->cut = key == 0x2d;
                b->status = b->cut ? "Перейдите в нужную папку и нажмите Ctrl+V" : "Файл выбран для копирования";
            } else b->status = "Копирование папок пока недоступно";
            ++b->revision;
        }
        if (key == 0x2f && b->clip >= 0) {
            const struct sfs_entry *e = sfs_entry_at(&b->disk, (unsigned)b->clip);
            if (e) {
                enum sfs_result r = b->cut ? sfs_move(&b->disk, &b->work, (unsigned)b->clip, b->folder, e->name) :
                    sfs_copy(&b->disk, &b->work, (unsigned)b->clip, b->folder, e->name, 0);
                b->status = r == SFS_OK ? (b->cut ? "Перемещено" : "Файл скопирован") : error_text(r);
                if (r == SFS_OK) {
                    arch_puts(b->cut ? "STYLEOS DESKTOP STYLEFS MOVE\n" : "STYLEOS DESKTOP STYLEFS COPY\n");
                    if (b->cut) b->clip = -1;
                }
                ++b->revision;
            }
        }
    } else if (key == 0x153 && b->selected >= 0) {
        if (b->selected == protected_entry) b->status = "Сначала закройте текстовый редактор";
        else {
            b->delete_id = b->selected;
            b->delete_generation = b->disk.generation;
            b->status = "Удалить без корзины? Enter / Esc";
        }
        ++b->revision;
    } else if (key == 0x3c) start_name(b, RENAME);
    else if (key == 0x148 || key == 0x150) {
        int step = b->grid ? grid_columns(area) : 1;
        change_selection(b, area, key == 0x148 ? -step : step);
    } else if (b->grid && (key == 0x14b || key == 0x14d))
        change_selection(b, area, key == 0x14b ? -1 : 1);
    else if (key == 0x149 || key == 0x151) {
        int step = (int)visible_rows(b, area);
        change_selection(b, area, key == 0x149 ? -step : step);
    } else if (key == 0x0e) go_parent(b);
    else if (key == 0x1c || key == 0x11c) return open_selected(b);
    return DESK_FS_NONE;
}

static int row_at(const struct desk_fs *b, struct desk_box a, int x, int y)
{
    if (b->computer || b->name_edit || b->delete_id >= 0 || x < sidebar(a) + 10 || x >= a.w - 14 ||
        y < FILE_TOP || y >= a.h - FOOT) return DESK_FS_NONE;
#ifdef STYLEOS_DESKTOP_UPDATE
    if (b->update_id >= 0 || desk_import_active()) return DESK_FS_NONE;
#endif
    unsigned row = (unsigned)(y - FILE_TOP) / ROW_H;
    if (b->grid) {
        unsigned col = (unsigned)(x - sidebar(a) - 10) / (unsigned)grid_width(a);
        if (col >= (unsigned)grid_columns(a)) return DESK_FS_NONE;
        row = (unsigned)(y - FILE_TOP) / GRID_H * (unsigned)grid_columns(a) + col;
    }
    if (row >= visible_rows(b, a)) return DESK_FS_NONE;
    int ids[SFS_ENTRIES];
    unsigned n = rows(b, ids);
    return row + b->first < n ? ids[b->first + row] : DESK_FS_NONE;
}

int desk_fs_hit_item(const struct desk_fs *b, struct desk_box a, int x, int y)
{ return row_at(b, a, x - a.x, y - a.y); }

int desk_fs_open_item(struct desk_fs *b, int item)
{
    if (item < 0 || item != b->selected) return DESK_FS_NONE;
    return open_selected(b);
}

int desk_fs_drag_source(const struct desk_fs *b, struct desk_box a, int x, int y)
{
    int id = desk_fs_hit_item(b, a, x, y);
    const struct sfs_entry *e = id < 0 ? 0 : sfs_entry_at(&b->disk, (unsigned)id);
    return e && sfs_scope_visible(&b->disk,(unsigned)id) && e->type == SFS_FILE && e->size && e->size <= DESK_STYLEFS_TEXT_LIMIT && txt_name(e->name) ? id : DESK_FS_NONE;
}

static struct desk_box action_panel(struct desk_box a)
{
    int x = sidebar(a) + 13;
    return (struct desk_box){x, a.h - FOOT - 100, a.w - x - 14, 100};
}

static bool in_button(int x, int y, int l, int t, int w, int h)
{ return x >= l && x < l + w && y >= t && y < t + h; }

static int search_left(struct desk_box a) { return a.w - (a.w >= 760 ? 208 : 182); }

static bool action_open(const struct desk_fs *b)
{
#ifdef STYLEOS_DESKTOP_UPDATE
    if (b->update_id >= 0) return true;
#endif
    return b->name_edit || b->delete_id >= 0;
}

int desk_fs_click(struct desk_fs *b, struct desk_box a, int x, int y, int protected_entry)
{
    if (!b->disk.mounted) return DESK_FS_NONE;
    x -= a.x;
    y -= a.y;
    if (action_open(b)) {
        struct desk_box p = action_panel(a);
        unsigned key = in_button(x, y, p.x + 13, p.y + 64, 130, 28) ? 0x1c :
                       in_button(x, y, p.x + 153, p.y + 64, 110, 28) ? 1 : 0;
        if (!key) return DESK_FS_NONE;
#ifdef STYLEOS_DESKTOP_UPDATE
        if (b->update_id >= 0) update_key(b, key);
        else
#endif
        if (b->name_edit) name_key(b, key, 0);
        else delete_key(b, key, protected_entry);
        return DESK_FS_NONE;
    }
    int side = sidebar(a), local = x - side;
#ifdef STYLEOS_DESKTOP_UPDATE
    if (desk_import_active()) {
        if (local >= 152 && local < 257 && y >= 90 && y < 120) desk_import_cancel(b);
        return DESK_FS_NONE;
    }
#endif
    if (b->searching) { b->searching = false; b->status = ""; ++b->revision; }
    if (y >= 48 && y < 79) {
        if (!b->computer && x >= search_left(a) && x < a.w - 14) {
            if (x >= a.w - 43 && b->query_length) (void)search_key(b, 1, 0);
            else {
                b->searching = true;
                b->status = "Введите имя файла. Enter / Esc";
                ++b->revision;
            }
        } else if (x >= 12 && x < 49) history_step(b, false);
        else if (x >= 53 && x < 90) history_step(b, true);
        else if (x >= 94 && x < 131) go_parent(b);
    } else if (x >= 8 && x < side) {
        if (y >= SIDE_ROWS && y < SIDE_ROWS + ROW_H * (int)(sizeof(places)/sizeof(*places))) {
            int id = find_folder(b, places[(y - SIDE_ROWS) / ROW_H]);
            if (id >= 0) browse(b, (unsigned)id);
#ifdef STYLEOS_DESKTOP_UPDATE
        } else if (y >= UPDATE_ROW && y < UPDATE_ROW + ROW_H) {
            if (!desk_update_probe()) { b->status = desk_update_status(); ++b->revision; }
            else {
                int dir = find_folder(b, "Обновления");
                if (dir < 0) {
                    unsigned created = 0;
                    enum sfs_result r = sfs_mkdir(&b->disk, &b->work, SFS_ROOT, "Обновления", &created);
                    if (r != SFS_OK) { b->status = error_text(r); ++b->revision; return DESK_FS_NONE; }
                    dir = (int)created;
                }
                browse(b, (unsigned)dir);
                arch_puts("STYLEOS DESKTOP UPDATES FOLDER\n");
            }
#endif
        } else if (y >= COMPUTER_ROW && y < COMPUTER_ROW + ROW_H) browse(b, COMPUTER);
        else if (y >= SYSTEM_ROW && y < SYSTEM_ROW + ROW_H) {
#ifdef STYLEOS_DESKTOP_UPDATE
            if (desk_update_probe()) { browse(b, COMPUTER); b->status = "Системный диск защищён от записи через Проводник"; }
            else
#endif
                browse(b, SFS_ROOT);
        } else if (y >= DATA_ROW && y < DATA_ROW + ROW_H) browse(b, SFS_ROOT);
    } else if (b->computer) {
        int card = (a.w - side - 43) / 2;
        if (card > 220) card = 220;
        if (y >= 126 && y < 186) {
#ifdef STYLEOS_DESKTOP_UPDATE
            if (desk_update_probe()) {
                if (local >= 14 && local < 14 + card) {
                    b->status = "Системный диск защищён от записи через Проводник";
                    ++b->revision;
                } else if (local >= 28 + card && local < 28 + card * 2) browse(b, SFS_ROOT);
            } else
#endif
                if (local >= 14 && local < 14 + card) browse(b, SFS_ROOT);
        }
    } else if (y >= 90 && y < 120) {
        if (x >= a.w - 48 && x < a.w - 14) {
            b->grid = true;
            b->first = 0;
            ++b->revision;
        } else if (x >= a.w - 86 && x < a.w - 52) {
            b->grid = false;
            b->first = 0;
            ++b->revision;
        } else if (local >= 13 && local < 145) start_name(b, MAKE_DIR);
        else if (local >= 152 && local < 257) {
#ifdef STYLEOS_DESKTOP_UPDATE
            if (b->update_view) { desk_import_begin(b); return DESK_FS_NONE; }
#endif
            return DESK_FS_NEW_TEXT;
        }
        else if (local >= 267 && local < 347) return open_selected(b);
    } else {
        int id = row_at(b, a, x, y);
        if (id >= 0) { b->selected = id; ++b->revision; }
    }
    return DESK_FS_NONE;
}

static void button(struct desk_box a, int x, int y, int w, const char *label, bool enabled)
{
    desk_round((struct desk_box){a.x + x, a.y + y, w, 28}, BUTTON);
    desk_text(a.x + x + 7, a.y + y + 4, a.x + x + w, label, enabled ? TEXT : DISABLED);
}

static void decimal(char *out, unsigned value)
{
    unsigned divisor = 1;
    while (divisor <= value / 10) divisor *= 10;
    do { *out++ = (char)('0' + value / divisor); value %= divisor; divisor /= 10; } while (divisor);
    *out = 0;
}

static void active_row(int x, int y, int w, int h)
{
    desk_round((struct desk_box){x, y, w, h}, SELECT);
    desk_fill(x - 3, y + h - 12, 6, 9, 0xd19247);
    desk_fill(x + 3, y + h - 19, 6, 9, AMBER);
}

static void side_icon(int x, int y, unsigned id, uint32_t color)
{
    switch (id) {
    case 0: case 6:  /* Downloads / Updates. */
        desk_outline((struct desk_box){x + 2, y + 7, 17, 12}, color);
        desk_line(x + 10, y + 2, x + 10, y + 15, color);
        desk_line(x + 7, y + 12, x + 10, y + 15, color);
        desk_line(x + 10, y + 15, x + 13, y + 12, color);
        break;
    case 2:
        desk_outline((struct desk_box){x + 1, y + 3, 18, 15}, color);
        desk_line(x + 2, y + 15, x + 8, y + 9, color);
        desk_line(x + 8, y + 9, x + 15, y + 16, color);
        desk_fill(x + 13, y + 6, 2, 2, color);
        break;
    case 3:
        desk_fill(x + 8, y + 4, 2, 12, color);
        desk_fill(x + 9, y + 4, 7, 2, color);
        desk_round((struct desk_box){x + 3, y + 13, 7, 5}, color);
        break;
    case 4:
        desk_outline((struct desk_box){x + 1, y + 3, 18, 15}, color);
        desk_line(x + 8, y + 6, x + 8, y + 15, color);
        desk_line(x + 8, y + 6, x + 14, y + 10, color);
        desk_line(x + 14, y + 10, x + 8, y + 15, color);
        break;
    case 7:
        desk_outline((struct desk_box){x + 1, y + 2, 19, 13}, color);
        desk_fill(x + 9, y + 15, 3, 3, color);
        desk_fill(x + 6, y + 18, 9, 1, color);
        break;
    default:
        desk_icon(x, y, 20, id == 1 ? DESK_PAPER : DESK_FOLDER, color);
        break;
    }
}

static void view_button(struct desk_box a, bool grid, bool selected)
{
    int x = a.x + a.w - (grid ? 48 : 86), y = a.y + 90;
    desk_round((struct desk_box){x, y, 34, 28}, selected ? SELECT : BUTTON);
    uint32_t rgb = selected ? AMBER : DIM;
    if (grid) {
        for (int i = 0; i < 4; ++i)
            desk_outline((struct desk_box){x + 9 + i % 2 * 9, y + 6 + i / 2 * 9, 7, 7}, rgb);
    } else for (int i = 0; i < 3; ++i) {
        desk_fill(x + 8, y + 7 + i * 6, 3, 2, rgb);
        desk_fill(x + 14, y + 7 + i * 6, 13, 2, rgb);
    }
}

static const char *kind(const struct sfs_entry *e)
{
    if (e->type == SFS_DIRECTORY) return "Папка";
    if (txt_name(e->name)) return "Текст";
#ifdef STYLEOS_DESKTOP_UPDATE
    if (update_name(e->name)) return "Пакет";
#endif
    return "Файл";
}

static void volume_label(char out[40], const struct desk_fs *b)
{
    const char prefix[] = "StyleFS v0 / ";
    unsigned end = sizeof(prefix) - 1;
    for (unsigned i = 0; i < end; ++i) out[i] = prefix[i];
    out[9] = (char)('0' + b->disk.format);
    decimal(out + end, b->disk.io.sectors / 2048);
    while (out[end]) ++end;
    const char unit[] = " МиБ";
    for (unsigned i = 0; i < sizeof(unit); ++i) out[end + i] = unit[i];
}

static void disk_card(int x, int y, int w, const char *name, const char *sub)
{
    struct desk_box card = {x, y, w, 60};
    desk_round(card, 0x1b283a);
    desk_outline(card, LINE);
    desk_icon(x + 10, y + 17, 30, DESK_DRIVE, TEXT);
    desk_text(x + 52, y + 9, x + w - 6, name, TEXT);
    desk_text(x + 52, y + 32, x + w - 6, sub, DIM);
}

static void object_count(int x, int y, int right, unsigned n, bool disk)
{
    char count[14];
    decimal(count, n);
    x = desk_text(x, y, right, count, DIM);
    unsigned one = n % 10, ten = n % 100;
    const char *one_label = disk ? " диск" : " объект";
    const char *some = disk ? " диска" : " объекта";
    const char *many = disk ? " дисков" : " объектов";
    desk_text(x, y, right, ten >= 11 && ten <= 14 ? many : one == 1 ? one_label :
              one >= 2 && one <= 4 ? some : many, DIM);
}

static void file_size(int x, int y, int right, unsigned size)
{
    char text[16];
    const char *unit = " Б";
    if (size >= 1024 * 1024) { size = (size + 1024 * 1024 - 1) / (1024 * 1024); unit = " МиБ"; }
    else if (size >= 1024) { size = (size + 1023) / 1024; unit = " КиБ"; }
    decimal(text, size);
    x = desk_text(x, y, right, text, DIM);
    desk_text(x, y, right, unit, DIM);
}

void desk_fs_draw(const struct desk_fs *b, struct desk_box a)
{
    int right = a.x + a.w - 14, side = sidebar(a), left = a.x + side;
    int footer = a.y + a.h - FOOT;
    unsigned count = 0;
    if (b->disk.mounted) {
        bool system = false;
#ifdef STYLEOS_DESKTOP_UPDATE
        system = desk_update_probe();
#endif
        const uint32_t muted = 0x35455b;
        for (int i = 0; i < 3; ++i) {
            int x = a.x + 30 + i * 41, y = a.y + 63;
            uint32_t color = (i == 0 ? b->history_at > 0 : i == 1 ?
                              b->history_at + 1 < b->history_count : !b->computer && b->folder) ? TEXT : DISABLED;
            if (i < 2) {
                int dir = i == 0 ? -1 : 1;
                desk_line(x - 8, y, x + 8, y, color);
                desk_line(x + dir * 8, y, x + dir * 2, y - 6, color);
                desk_line(x + dir * 8, y, x + dir * 2, y + 6, color);
            } else {
                desk_line(x, y - 7, x, y + 9, color);
                desk_line(x, y - 7, x - 6, y - 1, color);
                desk_line(x, y - 7, x + 6, y - 1, color);
            }
        }
        desk_fill(a.x + 135, a.y + 51, 1, 25, muted);
        int search_x = a.x + search_left(a), path_right = search_x - 12;
        struct desk_box search = {search_x, a.y + 48, right - search_x, 29};
        desk_outline(search, b->searching ? AMBER : LINE);
        desk_outline((struct desk_box){search_x + 9, a.y + 56, 10, 10}, DIM);
        desk_line(search_x + 17, a.y + 64, search_x + 22, a.y + 69, DIM);
        desk_text(search_x + 30, a.y + 53, right - (b->query_length ? 28 : 3),
                  b->query_length ? b->query : b->computer ? "Поиск" : "Поиск в папке",
                  b->query_length ? TEXT : DISABLED);
        if (b->query_length) {
            desk_line(right - 16, a.y + 59, right - 8, a.y + 67, DIM);
            desk_line(right - 8, a.y + 59, right - 16, a.y + 67, DIM);
        }

        int xx = desk_text(a.x + 151, a.y + 53, path_right, b->computer ? "Мой компьютер" : "Данные", TEXT);
        if (!b->computer && b->folder) {
            unsigned chain[SFS_ENTRIES], n = 0;
            for (unsigned id = b->folder; id && n < SFS_ENTRIES; id = b->disk.entries[id].parent) chain[n++] = id;
            while (n && xx < path_right) {
                xx = desk_text(xx, a.y + 53, path_right, " > ", DIM);
                xx = desk_text(xx, a.y + 53, path_right, b->disk.entries[chain[--n]].name, TEXT);
            }
        }
        desk_fill(a.x + 1, a.y + 83, a.w - 2, 1, LINE);
        desk_fill(left, a.y + 84, 1, a.h - 85, LINE);
        desk_text(a.x + 16, a.y + 94, left - 6, "Быстрый доступ", TEXT);
        for (unsigned i = 0; i < sizeof(places)/sizeof(*places); ++i) {
            int y = a.y + SIDE_ROWS + (int)i * ROW_H, target = find_folder(b, places[i]);
            if (!b->computer && !b->update_view && target >= 0 && b->folder == (unsigned)target)
                active_row(a.x + 9, y, side - 16, ROW_H - 2);
            side_icon(a.x + 18, y + 4, i, target >= 0 ? TEXT : DISABLED);
            desk_text(a.x + 45, y + 5, left - 8, places[i], target >= 0 ? TEXT : DISABLED);
        }
        if (b->update_view) active_row(a.x + 9, a.y + UPDATE_ROW, side - 16, ROW_H - 2);
        side_icon(a.x + 18, a.y + UPDATE_ROW + 4, 6, system ? TEXT : DISABLED);
        desk_text(a.x + 45, a.y + UPDATE_ROW + 5, left - 6, "Обновления", system ? TEXT : DISABLED);
        desk_fill(a.x + 12, a.y + 341, side - 24, 1, LINE);
        if (b->computer) active_row(a.x + 9, a.y + COMPUTER_ROW, side - 16, ROW_H - 1);
        side_icon(a.x + 18, a.y + COMPUTER_ROW + 5, 7, TEXT);
        desk_text(a.x + 45, a.y + COMPUTER_ROW + 4, left - 6, "Мой компьютер", TEXT);
        desk_icon(a.x + 36, a.y + SYSTEM_ROW + 4, 22, DESK_DRIVE, DIM);
        desk_text(a.x + 67, a.y + SYSTEM_ROW + 5, left - 6, system ? "Диск 1" : "Диск данных", DIM);
        if (system) {
            desk_icon(a.x + 36, a.y + DATA_ROW + 4, 22, DESK_DRIVE, DIM);
            desk_text(a.x + 67, a.y + DATA_ROW + 5, left - 6, "Диск 2", DIM);
        }
        if (b->computer) {
            desk_text(left + 15, a.y + 94, right, "Диски", TEXT);
            char volume[40];
            volume_label(volume, b);
            int w = (a.w - side - 43) / 2;
            if (w > 220) w = 220;
            if (system) {
                disk_card(left + 14, a.y + 126, w, "Диск 1", "Системный");
                disk_card(left + 28 + w, a.y + 126, w, "Диск 2 / Данные", volume);
            } else disk_card(left + 14, a.y + 126, w, "Диск данных", volume);
            desk_text(left + 15, a.y + 215, right, "Внешние устройства", TEXT);
            desk_text(left + 15, a.y + 248, right, "Нет доступных устройств", DIM);
            count = system ? 2 : 1;
        } else {
            button(a, side + 13, 91, 132, "+ Новая папка", true);
#ifdef STYLEOS_DESKTOP_UPDATE
            if (b->update_view) button(a, side + 152, 91, 105, desk_import_active() ? "Отмена" : "С ПК", true);
            else
#endif
                button(a, side + 152, 91, 105, "+ Текст", true);
            button(a, side + 267, 91, 80, "Открыть", b->selected >= 0);
            view_button(a, false, !b->grid);
            view_button(a, true, b->grid);
            desk_fill(left + 12, a.y + 125, right - left - 12, 1, LINE);
            int ids[SFS_ENTRIES];
            count = rows(b, ids);
            unsigned shown = visible_rows(b, a);
            if (b->grid) {
                int cols = grid_columns(a), w = grid_width(a);
                for (unsigned i = 0; i < shown && b->first + i < count; ++i) {
                    int id = ids[b->first + i], x = left + 10 + (int)i % cols * w;
                    int y = a.y + FILE_TOP + (int)i / cols * GRID_H;
                    if (y + 72 > footer) break;
                    const struct sfs_entry *e = &b->disk.entries[id];
                    if (id == b->selected) active_row(x, y, w - 6, 72);
                    desk_icon(x + (w - 42) / 2, y + 4, 36,
                              e->type == SFS_DIRECTORY ? DESK_FOLDER : DESK_PAPER,
                              e->type == SFS_DIRECTORY ? AMBER : TEXT);
                    desk_text(x + 7, y + 44, x + w - 10, e->name, TEXT);
                }
            } else {
                int type = right - 157, size = right - 74;
                desk_text(left + 16, a.y + 130, type - 12, "Имя", DIM);
                desk_text(type, a.y + 130, size - 8, "Тип", DIM);
                desk_text(size, a.y + 130, right, "Размер", DIM);
                desk_fill(left + 12, a.y + 152, right - left - 12, 1, muted);
                for (unsigned i = 0; i < shown && b->first + i < count; ++i) {
                    int id = ids[b->first + i], y = a.y + FILE_TOP + (int)i * ROW_H;
                    const struct sfs_entry *e = &b->disk.entries[id];
                    if (id == b->selected) active_row(left + 10, y, right - left - 10, ROW_H - 1);
                    desk_icon(left + 20, y + 4, 22, e->type == SFS_DIRECTORY ? DESK_FOLDER : DESK_PAPER,
                              e->type == SFS_DIRECTORY ? AMBER : TEXT);
                    desk_text(left + 55, y + 5, type - 12, e->name, TEXT);
                    desk_text(type, y + 5, size - 8, kind(e), DIM);
                    if (e->type == SFS_FILE) file_size(size, y + 5, right, e->size);
                    else desk_fill(size + 2, y + 15, 12, 1, DIM);
                }
            }
            if (!count) desk_text(left + 20, a.y + FILE_TOP + 10, right,
                                  b->query_length ? "Ничего не найдено" : "Папка пуста", DIM);
        }
        if (action_open(b)) {
            struct desk_box p = action_panel(a);
            struct desk_box screen = {a.x + p.x, a.y + p.y, p.w, p.h};
            const char *prompt, *target = "", *action;
#ifdef STYLEOS_DESKTOP_UPDATE
            if (b->update_id >= 0) {
                const struct sfs_entry *e = sfs_entry_at(&b->disk, (unsigned)b->update_id);
                prompt = "Установить обновление?";
                if (e) target = e->name;
                action = "Установить";
            } else
#endif
            if (b->delete_id >= 0) {
                const struct sfs_entry *e = sfs_entry_at(&b->disk, (unsigned)b->delete_id);
                prompt = "Удалить без корзины?";
                if (e) target = e->name;
                action = "Удалить";
            } else {
                prompt = b->name_edit == MAKE_DIR ? "Имя новой папки" : "Новое имя";
                target = b->name;
                action = b->name_edit == MAKE_DIR ? "Создать" : "Сохранить";
            }
            desk_round(screen, 0x111a25);
            desk_outline(screen, AMBER);
            desk_text(screen.x + 13, screen.y + 8, right - 8, prompt, TEXT);
            int tx = desk_text(screen.x + 13, screen.y + 33, right - 14, target, DIM);
            if (b->name_edit && tx < right - 13) desk_fill(tx, screen.y + 33, 1, 20, AMBER);
            button(a, p.x + 13, p.y + 64, 130, action, true);
            button(a, p.x + 153, p.y + 64, 110, "Отмена", true);
        }
        desk_fill(left + 12, a.y + a.h - 34, right - left - 12, 1, LINE);
    }
    int status = left + 14;
    if (b->disk.mounted && !b->status[0]) object_count(status, a.y + a.h - 26, right, count, b->computer);
    else desk_text(status, a.y + a.h - 26, right, b->status, DIM);
}

bool desk_fs_read_text(struct desk_fs *b, unsigned id, char *data, size_t *size)
{
    const struct sfs_entry *e = sfs_entry_at(&b->disk, id);
    if (!e || e->size > DESK_STYLEFS_TEXT_LIMIT || !txt_name(e->name) ||
        sfs_read(&b->disk, id, (uint8_t *)data, DESK_STYLEFS_TEXT_LIMIT) != SFS_OK) return false;
    for (size_t off = 0; off < e->size;) {
        size_t used;
        uint32_t cp = desk_utf8(data + off, e->size - off, &used);
        if (!used || (cp < 32 && cp != '\r' && cp != '\n' && cp != '\t') ||
            (cp == 0xfffd && used == 1)) return false;
        off += used;
    }
    *size = e->size;
    return true;
}

/* Separate native editor and Notes drafts get separate on-disk names.
 * Existing documents preserve their own entry IDs and never change names. */
static bool save_named_text(struct desk_fs *b, unsigned folder, int *id,
                            const char *data, size_t length, const char *prefix)
{
    enum sfs_result r = SFS_FULL;
    if (*id >= 0) r = sfs_write(&b->disk, &b->work, (unsigned)*id, (const uint8_t *)data, length);
    else for (unsigned n = 1; n <= SFS_ENTRIES; ++n) {
        char name[48];
        unsigned end = 0;
        for (unsigned k = 0; prefix[k]; ++k) name[end++] = prefix[k];
        if (n > 1) {
            name[end++] = ' ';
            decimal(name + end, n);
            while (name[end]) ++end;
        }
        const char *extension = ".txt";
        unsigned k = 0;
        do { name[end + k] = extension[k]; } while (extension[k++]);
        unsigned created;
        r = sfs_create(&b->disk, &b->work, folder, name,
                       (const uint8_t *)data, length, &created);
        if (r == SFS_EXISTS) continue;
        if (r == SFS_OK) *id = (int)created;
        break;
    }
    if (r == SFS_OK) ++b->revision;
    else b->status = error_text(r);
    return r == SFS_OK;
}

bool desk_fs_save_text(struct desk_fs *b, unsigned folder, int *id, const char *data, size_t length)
{
    return save_named_text(b, folder, id, data, length, "Текст");
}

bool desk_fs_save_note(struct desk_fs *b, unsigned folder, int *id, const char *data, size_t length)
{
    return save_named_text(b, folder, id, data, length, "Заметка");
}

#ifdef STYLEOS_LIVE_VOLUME_39
#include <styleos/desktop_paged_view.h>
/* Use the existing Explorer palette, sidebar, row sizes and columns.
 * This is a read-only first-page adapter, not a redesign. No disk access
 * in paint, no legacy ID conversion, no fabricated file rows. */
static void size41(int x,int y,int right,uint64_t bytes){
 char text[24],rev[24];unsigned n=0;
 const char *unit=" Б";
 if(bytes>=UINT64_C(1099511627776)){bytes=bytes/UINT64_C(1099511627776)+(bytes%UINT64_C(1099511627776)!=0);unit=" ТиБ";}
 else if(bytes>=UINT64_C(1073741824)){bytes=bytes/UINT64_C(1073741824)+(bytes%UINT64_C(1073741824)!=0);unit=" ГиБ";}
 else if(bytes>=UINT64_C(1048576)){bytes=bytes/UINT64_C(1048576)+(bytes%UINT64_C(1048576)!=0);unit=" МиБ";}
 else if(bytes>=1024){bytes=bytes/1024+(bytes%1024!=0);unit=" КиБ";}
 do{rev[n++]=(char)('0'+bytes%10);bytes/=10;}while(bytes);
 for(unsigned i=0;i<n;i++)text[i]=rev[n-i-1];
 text[n]=0;
 x=desk_text(x,y,right,text,DIM);desk_text(x,y,right,unit,DIM);
}
static unsigned shown42(struct desk_box a){
 return a.h>FILE_TOP+FOOT?(unsigned)(a.h-FILE_TOP-FOOT)/ROW_H:0;
}
static unsigned first42(const struct dp41_view *v,struct desk_box a){
 unsigned n=shown42(a);
 return n&&v->selected>=n?v->selected-n+1:0;
}
bool desk_fs_paged_click(const struct dp41_view *v,struct desk_box a,int x,int y){
 if(!v||!v->valid)return false;
 int side=sidebar(a),left=a.x+side,right=a.x+a.w-14;
 if(x>=a.x+12&&x<a.x+115&&y>=a.y+48&&y<a.y+77)return s42_navigate(0x0e);
 if(x>=left+267&&x<left+347&&y>=a.y+91&&y<a.y+119)return s42_select(v->selected,true);
 if(x<left+10||x>=right||y<a.y+FILE_TOP||y>=a.y+a.h-FOOT)return false;
 unsigned row=(unsigned)(y-a.y-FILE_TOP)/ROW_H;
 if(row>=shown42(a))return false;
 return s42_select(first42(v,a)+row,false);
}
void desk_fs_paged_root_draw(const struct dp41_view *v,struct desk_box a){
 if(!v||!v->valid||v->count>DP41_ROWS)return;
 int side=sidebar(a),left=a.x+side,right=a.x+a.w-14,footer=a.y+a.h-FOOT;
 desk_text(a.x+16,a.y+53,a.x+133,"Вверх",v->parent!=1?TEXT:DISABLED);
 desk_text(a.x+151,a.y+53,right,v->parent==1?"Данные":"Данные / Папка",TEXT);
 desk_fill(a.x+1,a.y+83,a.w-2,1,LINE);
 desk_fill(left,a.y+84,1,a.h-85,LINE);
 desk_text(a.x+16,a.y+94,left-6,"Быстрый доступ",TEXT);
 for(unsigned i=0;i<sizeof places/sizeof *places;i++){
  int y=a.y+SIDE_ROWS+(int)i*ROW_H;
  side_icon(a.x+18,y+4,i,DISABLED);
  desk_text(a.x+45,y+5,left-8,places[i],DISABLED);
 }
 desk_fill(a.x+12,a.y+341,side-24,1,LINE);
 desk_icon(a.x+36,a.y+SYSTEM_ROW+4,22,DESK_DRIVE,DIM);
 desk_text(a.x+67,a.y+SYSTEM_ROW+5,left-6,"Диск данных",TEXT);
 button(a,side+13,91,132,"+ Новая папка",false);
 button(a,side+152,91,105,"+ Текст",false);
 button(a,side+267,91,80,"Открыть",v->count&&v->rows[v->selected].entry.kind==SD64_DIRECTORY);
 int type=right-157,size=right-74;
 desk_text(left+16,a.y+130,type-12,"Имя",DIM);
 desk_text(type,a.y+130,size-8,"Тип",DIM);
 desk_text(size,a.y+130,right,"Размер",DIM);
 desk_fill(left+12,a.y+152,right-left-12,1,LINE);
 unsigned shown=0;
 unsigned first=first42(v,a);
 for(unsigned i=first;i<v->count;i++){
  int y=a.y+FILE_TOP+(int)(i-first)*ROW_H;if(y+ROW_H>footer)break;
  const struct po64_row *row=&v->rows[i];
  char name[SD64_NAME+1];unsigned n=row->entry.key.length;
  if(!n||n>SD64_NAME)continue;
  for(unsigned j=0;j<n;j++)name[j]=(char)row->entry.key.name[j];
  name[n]=0;
  bool dir=row->entry.kind==SD64_DIRECTORY;
  if(i==v->selected)active_row(left+10,y,right-left-10,ROW_H-1);
  desk_icon(left+20,y+4,22,dir?DESK_FOLDER:DESK_PAPER,dir?AMBER:TEXT);
  desk_text(left+55,y+5,type-12,name,TEXT);
  desk_text(type,y+5,size-8,dir?"Папка":"Файл",DIM);
  if(!dir)size41(size,y+5,right,row->bytes);
  ++shown;
 }
 if(!v->count)desk_text(left+20,a.y+FILE_TOP+10,right,"Папка пуста",DIM);
 desk_fill(left+12,a.y+a.h-34,right-left-12,1,LINE);
 desk_text(left+14,a.y+a.h-26,right,v->more||v->first?"Только чтение — PgUp/PgDn":"Только чтение — Enter / Backspace",DIM);
 static bool reported;
 if(shown&&!reported){reported=true;arch_puts("SFS41 EXPLORER ROWS\n");}
 static uint64_t last_parent;
 if(shown&&last_parent!=v->parent){
  last_parent=v->parent;
  arch_puts(v->parent==1?"SFS42 ROOT ROWS\n":"SFS42 CHILD ROWS\n");
 }
}
#endif
