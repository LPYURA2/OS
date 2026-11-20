#include <styleos/desktop_stylefs.h>
#include <styleos/arch.h>

enum { TEXT = 0xf3f0e9, DIM = 0xaeb8c7, AMBER = 0xe6ad63, BUTTON = 0x253143,
       LINE = 0x49596f, SELECT = 0x76572f, DISABLED = 0x748297,
       FILE_TOP = 154, ROW_H = 31, FOOT = 43, SIDEBAR = 222,
       SIDE_ROWS = 122, COMPUTER_ROW = 350, VOLUME_ROW = 384,
       MAKE_DIR = 1, RENAME = 2, COMPUTER = SFS_ENTRIES };
static const char *const places[] = {"Загрузки", "Документы", "Изображения", "Музыка", "Видео", "Проекты"};

static int sidebar(struct desk_box a) { return a.w / 3 < SIDEBAR ? a.w / 3 : SIDEBAR; }

static bool read_sector(void *context, uint32_t lba, uint8_t out[512])
{ (void)context; return desk_ata_read(lba, out); }
static bool write_sector(void *context, uint32_t lba, const uint8_t in[512])
{ (void)context; return desk_ata_write(lba, in); }
static bool flush_disk(void *context) { (void)context; return desk_ata_flush(); }

void desk_fs_mount(struct desk_fs *b, uint32_t sectors)
{
    b->mode = sectors == SFS_DISK_SECTORS;
    b->selected = b->clip = -1;
    b->history_count = 1;
    b->status = "F2: имя   Ctrl+C/V: копия";
    if (!b->mode) return;
    const struct sfs_io io = {0, sectors, read_sector, write_sector, flush_disk};
    enum sfs_result r = sfs_mount(&b->disk, &b->work, &io);
    if (r == SFS_OK) arch_puts("STYLEOS DESKTOP STYLEFS MOUNT\n");
    else {
        arch_puts("STYLEOS DESKTOP STYLEFS REFUSED\n");
        b->status = "Том StyleFS недоступен. Диск не изменён";
    }
}

static void show(struct desk_fs *b, unsigned id)
{
    b->computer = id == COMPUTER;
    if (!b->computer) b->folder = id;
    b->selected = -1;
    b->first = b->name_edit = 0;
    ++b->revision;
}

static void browse(struct desk_fs *b, unsigned id)
{
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

static unsigned rows(const struct desk_fs *b, int out[SFS_ENTRIES])
{
    if (!b->disk.mounted || b->computer) return 0;
    unsigned n = 0;
    for (unsigned type = SFS_DIRECTORY; type <= SFS_FILE; ++type)
        for (unsigned id = 1; id < SFS_ENTRIES; ++id) {
            const struct sfs_entry *e = sfs_entry_at(&b->disk, id);
            if (e && e->parent == b->folder && e->type == type) out[n++] = (int)id;
        }
    return n;
}

static unsigned visible_rows(struct desk_box area)
{ return area.h > FILE_TOP + FOOT ? (unsigned)(area.h - FILE_TOP - FOOT) / ROW_H : 0u; }

static void change_selection(struct desk_fs *b, struct desk_box area, int delta)
{
    int ids[SFS_ENTRIES], pos = -1;
    unsigned n = rows(b, ids), visible = visible_rows(area);
    if (!n || !visible) return;
    for (unsigned i = 0; i < n; ++i) if (ids[i] == b->selected) pos = (int)i;
    pos = pos < 0 ? 0 : pos + delta;
    if (pos < 0) pos = 0;
    if ((unsigned)pos >= n) pos = (int)n - 1;
    b->selected = ids[pos];
    if ((unsigned)pos < b->first) b->first = (unsigned)pos;
    if ((unsigned)pos >= b->first + visible) b->first = (unsigned)pos + 1 - visible;
    ++b->revision;
}

static bool txt_name(const char *name)
{
    unsigned n = 0;
    while (name[n]) ++n;
    if (n < 4 || name[n - 4] != '.') return false;
    return (name[n - 3] | 32) == 't' && (name[n - 2] | 32) == 'x' && (name[n - 1] | 32) == 't';
}

static int open_selected(struct desk_fs *b)
{
    if (b->selected < 0) return DESK_FS_NONE;
    const struct sfs_entry *e = sfs_entry_at(&b->disk, (unsigned)b->selected);
    if (!e) return DESK_FS_NONE;
    if (e->type == SFS_DIRECTORY) { browse(b, (unsigned)b->selected); return DESK_FS_NONE; }
    if (txt_name(e->name) && e->size <= DESK_TEXT_LIMIT) return b->selected;
    b->status = e->size > DESK_TEXT_LIMIT && txt_name(e->name) ? "Текст больше 4096 байт" : "Можно копировать. Нет приложения";
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
    case SFS_NAME: return "Недопустимое имя";
    case SFS_EXISTS: return "Такое имя уже есть";
    case SFS_FULL: return "Нет свободных записей";
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
        if (b->name_length + n <= SFS_NAME_MAX) {
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

int desk_fs_key(struct desk_fs *b, struct desk_box area, unsigned key, unsigned mods, uint32_t cp)
{
    if (!b->disk.mounted) return DESK_FS_NONE;
    if (b->computer) {
        if (!mods && (key == 0x1c || key == 0x11c)) browse(b, SFS_ROOT);
        return DESK_FS_NONE;
    }
    if (b->name_edit) { if (!(mods & (DESK_ALT | DESK_CTRL))) name_key(b, key, cp); return DESK_FS_NONE; }
    if (mods & DESK_ALT) return DESK_FS_NONE;
    if (mods & DESK_CTRL) {
        if (key == 0x31) start_name(b, MAKE_DIR);
        if (key == 0x2e && b->selected >= 0) {
            const struct sfs_entry *e = sfs_entry_at(&b->disk, (unsigned)b->selected);
            if (e && e->type == SFS_FILE) { b->clip = b->selected; b->status = "Файл выбран для копирования"; }
            else b->status = "Копирование папок пока недоступно";
            ++b->revision;
        }
        if (key == 0x2f && b->clip >= 0) {
            const struct sfs_entry *e = sfs_entry_at(&b->disk, (unsigned)b->clip);
            if (e) {
                enum sfs_result r = sfs_copy(&b->disk, &b->work, (unsigned)b->clip, b->folder, e->name, 0);
                b->status = r == SFS_OK ? "Файл скопирован" : error_text(r);
                if (r == SFS_OK) arch_puts("STYLEOS DESKTOP STYLEFS COPY\n");
                ++b->revision;
            }
        }
    } else if (key == 0x3c) start_name(b, RENAME);
    else if (key == 0x148 || key == 0x150) change_selection(b, area, key == 0x148 ? -1 : 1);
    else if (key == 0x149 || key == 0x151) {
        int step = (int)visible_rows(area);
        change_selection(b, area, key == 0x149 ? -step : step);
    } else if (key == 0x0e) go_parent(b);
    else if (key == 0x1c || key == 0x11c) return open_selected(b);
    return DESK_FS_NONE;
}

static int row_at(const struct desk_fs *b, struct desk_box a, int x, int y)
{
    if (b->computer || b->name_edit || x < sidebar(a) + 10 || x >= a.w - 12 || y < FILE_TOP)
        return DESK_FS_NONE;
    unsigned row = (unsigned)(y - FILE_TOP) / ROW_H;
    if (row >= visible_rows(a)) return DESK_FS_NONE;
    int ids[SFS_ENTRIES];
    unsigned n = rows(b, ids);
    return row + b->first < n ? ids[b->first + row] : DESK_FS_NONE;
}

int desk_fs_drag_source(const struct desk_fs *b, struct desk_box a, int x, int y)
{
    int id = row_at(b, a, x - a.x, y - a.y);
    const struct sfs_entry *e = id < 0 ? 0 : sfs_entry_at(&b->disk, (unsigned)id);
    return e && e->type == SFS_FILE && e->size && e->size <= DESK_TEXT_LIMIT && txt_name(e->name) ? id : DESK_FS_NONE;
}

int desk_fs_click(struct desk_fs *b, struct desk_box a, int x, int y)
{
    if (!b->disk.mounted || b->name_edit) return DESK_FS_NONE;
    x -= a.x;
    y -= a.y;
    int side = sidebar(a), local = x - side;
    if (y >= 48 && y < 79) {
        if (x >= 12 && x < 49) history_step(b, false);
        else if (x >= 53 && x < 90) history_step(b, true);
        else if (x >= 94 && x < 131) go_parent(b);
    } else if (x >= 8 && x < side) {
        if (y >= SIDE_ROWS && y < SIDE_ROWS + ROW_H * (int)(sizeof(places)/sizeof(*places))) {
            int id = find_folder(b, places[(y - SIDE_ROWS) / ROW_H]);
            if (id >= 0) browse(b, (unsigned)id);
        } else if (y >= COMPUTER_ROW && y < COMPUTER_ROW + ROW_H) browse(b, COMPUTER);
        else if (y >= VOLUME_ROW && y < VOLUME_ROW + ROW_H) browse(b, SFS_ROOT);
    } else if (b->computer) {
        if (local >= 14 && local < 238 && y >= 126 && y < 186) browse(b, SFS_ROOT);
    } else if (y >= 90 && y < 120) {
        if (local >= 13 && local < 145) start_name(b, MAKE_DIR);
        else if (local >= 152 && local < 260) return DESK_FS_NEW_TEXT;
        else if (local >= 267 && local < 355) return open_selected(b);
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

void desk_fs_draw(const struct desk_fs *b, struct desk_box a)
{
    int right = a.x + a.w - 14;
    if (b->disk.mounted) {
        int side = sidebar(a), left = a.x + side;
        button(a, 12, 49, 37, "<", b->history_at > 0);
        button(a, 53, 49, 37, ">", b->history_at + 1 < b->history_count);
        button(a, 94, 49, 37, "^", !b->computer && b->folder);
        int path_right = right;
        if (a.w >= 760) {
            path_right -= 182;
            desk_outline((struct desk_box){path_right + 8, a.y + 48, 174, 29}, LINE);
            desk_text(path_right + 19, a.y + 53, right, "Поиск в папке", DISABLED);
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
        desk_text(a.x + 15, a.y + 94, left - 6, "Быстрый доступ", TEXT);
        for (unsigned i = 0; i < sizeof(places)/sizeof(*places); ++i) {
            int y = a.y + SIDE_ROWS + (int)i * ROW_H, target = find_folder(b, places[i]);
            if (!b->computer && target >= 0 && b->folder == (unsigned)target)
                desk_round((struct desk_box){a.x + 8, y, side - 15, ROW_H - 2}, SELECT);
            desk_icon(a.x + 16, y + 4, 20, DESK_FOLDER, target >= 0 ? AMBER : DISABLED);
            desk_text(a.x + 45, y + 5, left - 8, places[i], target >= 0 ? TEXT : DISABLED);
        }
        desk_text(a.x + 17, a.y + 315, left - 6, "Обновления", DISABLED);
        desk_fill(a.x + 12, a.y + 341, side - 24, 1, LINE);
        if (b->computer) desk_round((struct desk_box){a.x + 8, a.y + COMPUTER_ROW, side - 15, ROW_H - 1}, SELECT);
        desk_text(a.x + 17, a.y + COMPUTER_ROW + 4, left - 6, "Мой компьютер", TEXT);
        desk_text(a.x + 27, a.y + VOLUME_ROW + 4, left - 6, "Диск данных", DIM);
        if (b->computer) {
            desk_text(left + 15, a.y + 94, right, "Диски", TEXT);
            desk_outline((struct desk_box){left + 14, a.y + 126, 224, 60}, LINE);
            desk_icon(left + 23, a.y + 143, 24, DESK_DRIVE, TEXT);
            desk_text(left + 58, a.y + 132, right, "Диск данных", TEXT);
            desk_text(left + 58, a.y + 155, right, "StyleFS 8 МиБ", DIM);
            desk_text(left + 15, a.y + 215, right, "Внешние устройства", TEXT);
            desk_text(left + 15, a.y + 248, right, "Нет подключённых устройств", DIM);
            desk_text(left + 14, a.y + a.h - 24, right, "1 доступный диск", DIM);
            return;
        }
        button(a, side + 13, 91, 132, "+ Папка", true);
        button(a, side + 152, 91, 108, "+ Текст", true);
        button(a, side + 267, 91, 88, "Открыть", b->selected >= 0);
        desk_fill(left + 12, a.y + 125, right - left - 12, 1, LINE);
        desk_text(left + 16, a.y + 130, right - 160, "Имя", DIM);
        desk_text(right - 146, a.y + 130, right - 73, "Тип", DIM);
        desk_text(right - 67, a.y + 130, right, "Размер", DIM);
        int ids[SFS_ENTRIES];
        unsigned n = rows(b, ids), shown = visible_rows(a);
        for (unsigned i = 0; i < shown && b->first + i < n; ++i) {
            int id = ids[b->first + i], y = a.y + FILE_TOP + (int)i * ROW_H;
            const struct sfs_entry *e = &b->disk.entries[id];
            if (id == b->selected) desk_round((struct desk_box){left + 10, y, right - left - 10, ROW_H - 1}, SELECT);
            desk_icon(left + 20, y + 4, 22, e->type == SFS_DIRECTORY ? DESK_FOLDER : DESK_PAPER,
                      e->type == SFS_DIRECTORY ? AMBER : TEXT);
            desk_text(left + 55, y + 5, right - 158, e->name, TEXT);
            desk_text(right - 146, y + 5, right - 74, e->type == SFS_DIRECTORY ? "Папка" : "Файл", DIM);
            if (e->type == SFS_FILE) { char size[12]; decimal(size, e->size); desk_text(right - 64, y + 5, right, size, DIM); }
        }
        if (!n) desk_text(left + 20, a.y + FILE_TOP + 10, right, "Папка пуста", DIM);
        if (b->name_edit) {
            int y = a.y + a.h - FOOT - 27;
            desk_round((struct desk_box){left + 13, y, right - left - 13, 41}, 0x111a25);
            desk_outline((struct desk_box){left + 13, y, right - left - 13, 41}, AMBER);
            desk_text(left + 22, y + 11, right - 8, b->name, TEXT);
        }
    }
    desk_text(a.x + sidebar(a) + 14, a.y + a.h - 24, right, b->status, DIM);
}

bool desk_fs_read_text(struct desk_fs *b, unsigned id, char *data, size_t *size)
{
    const struct sfs_entry *e = sfs_entry_at(&b->disk, id);
    if (!e || e->size > DESK_TEXT_LIMIT || !txt_name(e->name) ||
        sfs_read(&b->disk, id, (uint8_t *)data, DESK_TEXT_LIMIT) != SFS_OK) return false;
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

bool desk_fs_save_text(struct desk_fs *b, unsigned folder, int *id, const char *data, size_t length)
{
    enum sfs_result r = SFS_FULL;
    if (*id >= 0) r = sfs_write(&b->disk, &b->work, (unsigned)*id, (const uint8_t *)data, length);
    else for (unsigned n = 1; n <= SFS_ENTRIES; ++n) {
        char name[48] = "Текст";
        unsigned end = sizeof("Текст") - 1;
        if (n > 1) {
            name[end++] = ' ';
            decimal(name + end, n);
            while (name[end]) ++end;
        }
        const char *extension = ".txt";
        for (unsigned i = 0; extension[i]; ++i) name[end + i] = extension[i];
        unsigned created;
        r = sfs_create(&b->disk, &b->work, folder, name, (const uint8_t *)data, length, &created);
        if (r == SFS_EXISTS) continue;
        if (r == SFS_OK) *id = (int)created;
        break;
    }
    if (r == SFS_OK) ++b->revision;
    else b->status = error_text(r);
    return r == SFS_OK;
}
