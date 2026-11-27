#ifdef STYLEOS_DESKTOP_UPDATE
#include <styleos/system_update.h>
#include <styleos/desktop_import.h>
#endif
#include <styleos/arch.h>
#include <styleos/desktop_v0.h>
#include <styleos/desktop_stylefs.h>
#ifdef STYLEOS_LIVE_VOLUME_39
#include <styleos/desktop_paged_view.h>
#endif
#include <styleos/desktop_sessions.h>
#include <styleos/desktop_text_app.h>
#include <styleos/native_desktop_bridge.h>
#ifdef STYLEOS_OWNED_SHELL_865
#include <styleos/native_runtime_865.h>
#endif
#if defined(STYLEOS_DESKTOP_V0) || (defined(STYLEOS_TEST_DESKTOP_INTERACTIVE_863) || defined(STYLEOS_OWNED_SHELL_865))
#include <styleos/desktop_process_runtime.h>
#include <styleos/runtime_entry_858.h>
static void desktop_kernel_boundary864(void){
 if(!rt864_arch_shell_boundary()){
  arch_puts("STYLEOS DESKTOP RUNTIME BOUNDARY ERROR\n");arch_halt();
 }
}
#endif
#if (defined(STYLEOS_TEST_DESKTOP_INTERACTIVE_863) || defined(STYLEOS_OWNED_SHELL_865))
#include <styleos/desktop_interactive_863.h>
#endif

enum { FILES, NOTES, TASKS, ABOUT, NOTE_WINDOW, WINDOWS };
enum { LIGHT = 0xf3f0e9, MUTED = 0xaeb8c7, GOLD = 0xe6ad63, FOLDER = 0xe7be80,
       WINDOW = 0x171f2c, STROKE = 0x586272, PANEL = 0x101822, CELL = 11,
       LINE = 24, TITLE = 42, ROW = 45, CURSOR_W = 10, CURSOR_H = 17 };

static const char processes_name[] = "Процессы";
static const char *const names[WINDOWS] = {"Файлы", "Текстовый редактор", processes_name, "О системе", "Заметки"};
static const char *const folders[] = {"Файлы", "Документы", "Проекты"};
static const char *const files[] = {"Заметки.txt", "Заметки 2.txt", "Заметки 3.txt", "Заметки 4.txt"};
static const uint8_t icons[] = {DESK_FILE_MANAGER, DESK_PAPER, DESK_TASK, DESK_INFO, DESK_APP_NOTES};

static struct window {
    struct desk_box box, previous;
    bool open, minimized, maximized;
} win[WINDOWS];
static struct desk_store store;
static struct desk_fs filesystem;
static int width, height, mx, my, order[WINDOWS] = {ABOUT, TASKS, NOTE_WINDOW, NOTES, FILES};
static int drag = -1, drag_x, drag_y;
static bool mouse_down, menu, drag_moved;
static struct file_click {
    int id, x, y;
    unsigned revision;
    uint64_t at;
} last_file_click, current_file_click = {.id = -1};
static bool second_file_click;
static unsigned current_folder, input_flags;
static uint32_t data_sectors;
static char incoming[DESK_STYLEFS_TEXT_LIMIT];
#ifdef STYLEOS_LIVE_VOLUME_39
static bool paged_editor_open;
static char paged_editor_name[SD64_NAME+1];
#ifdef STYLEOS_LIVE_VOLUME_44
static struct po64_row paged_editor_row;
#endif
#endif
static bool russian, shell_refreshed;
/* No synthetic windows are created: the port registry is empty at boot. */
static struct ndb_tracker native_view;
#ifdef STYLEOS_OWNED_SHELL_865
static struct ndb_input native_input;
#endif
static void native_plot(void *unused,int x,int y,uint32_t rgb)
{ (void)unused;desk_fill(x,y,1,1,rgb); }
static void native_fill(void *unused,int x,int y,int w,int h,uint32_t rgb)
{ (void)unused;desk_fill(x,y,w,h,rgb); }

/* Editor and Notes are separate app sessions. Their buffers live outside the
 * compositor; rendering either window never switches the other app's draft. */
static int active(void);
static struct desk_editor_draft *rendering_draft;
static struct desk_editor_draft *current_draft(void)
{
    if (rendering_draft) return rendering_draft;
    return active() == NOTE_WINDOW ? &desk_sessions_global()->notes :
                                     &desk_sessions_global()->editor;
}
#if (defined(STYLEOS_TEST_DESKTOP_INTERACTIVE_863) || defined(STYLEOS_OWNED_SHELL_865))
static size_t note_length863(void){return desk_sessions_global()->notes.length;}
static uint64_t note_hash863(void){
    const struct desk_editor_draft *n=&desk_sessions_global()->notes;
    if(n->length>sizeof n->content)arch_halt();
    uint64_t hash=UINT64_C(14695981039346656037);
    for(size_t i=0;i<n->length;i++){hash^=(unsigned char)n->content[i];hash*=UINT64_C(1099511628211);}
    return hash;
}
static bool note_dirty863(void){return desk_sessions_global()->notes.dirty;}
static unsigned note_session863(void){return desk_sessions_pid(desk_sessions_global(),DESK_SESSION_NOTES);}
#endif
#if (defined(STYLEOS_TEST_DESKTOP_STORAGE_SERVICE_863) || defined(STYLEOS_OWNED_SHELL_865))
extern void s863_storage_document(bool,unsigned,const char *,size_t);
#endif
#ifdef STYLEOS_LIVE_VOLUME_39
/* Access actual member names before the legacy editor alias macros. */
static bool s43_editor_dirty(void){return desk_sessions_global()->editor.dirty;}
static unsigned s43_name_length(const struct po64_row *row){return row->entry.key.length;}
#endif
#define document (current_draft()->document)
#define doc_folder (current_draft()->doc_folder)
#define content (current_draft()->content)
#define length (current_draft()->length)
#define caret (current_draft()->caret)
#define scroll (current_draft()->scroll)
#define dirty (current_draft()->dirty)
#define notice (current_draft()->notice)
static char clock_text[] = "--:--";
static struct {
    int id, x, y;
    uint32_t generation;
    bool lifted;
} transfer = {.id = -1};

static bool data_writable(void) {
    return filesystem.mode ? filesystem.disk.mounted && filesystem.disk.io.write && filesystem.disk.io.flush : store.writable;
}
static bool data_available(void) {
#ifdef STYLEOS_LIVE_VOLUME_39
    return s41_root_view()!=0;
#else
    return filesystem.mode ? filesystem.disk.mounted : store.writable;
#endif
}

/* The updatable desktop has one visual identity. Media detection selects a
 * storage backend, never an older skin. Keep old standalone profiles intact. */
static bool modern_shell(void)
{
#if defined(STYLEOS_DESKTOP_UPDATE) || defined(STYLEOS_LIVE_VOLUME_39)
    return true;
#else
    return filesystem.mode;
#endif
}

static const char *data_description(void)
{
#ifdef STYLEOS_LIVE_VOLUME_39
#ifdef STYLEOS_LIVE_VOLUME_44
    if(s44_writable())return "StyleFS — чтение и запись";
#endif
    return s41_root_view()?"StyleFS — только чтение":"Диск данных не подключён";
#elif defined(STYLEOS_DESKTOP_UPDATE)
    if (!data_sectors) return "Диск данных не обнаружен";
    if (filesystem.disk.mounted) return filesystem.disk.format == 2 ? "Диск данных StyleFS v2" : "Диск данных StyleFS v1";
    if (store.writable) return "Диск заметок старого формата";
    return "Диск данных не распознан";
#else
    return data_writable() ? "Диск данных подключён" : "Нет диска данных для записи";
#endif
}

static const char *app_name(int id)
{
    if (modern_shell() && id == FILES) return "Проводник";
    if (modern_shell() && id == NOTES) return "Текстовый редактор";
    return names[id];
}
static unsigned app_icon(int id)
{
    static const uint8_t new_icons[] = {DESK_APP_EXPLORER, DESK_APP_EDITOR, DESK_APP_PROCESSES, DESK_INFO, DESK_APP_NOTES};
    return modern_shell() ? new_icons[id] : icons[id];
}

static int max(int a, int b) { return a > b ? a : b; }
static int min(int a, int b) { return a < b ? a : b; }
static int clamp(int a, int l, int h) { return min(max(a, l), max(l, h)); }
static bool contains(struct desk_box b, int x, int y)
{ return x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h; }

/* The panel and work area meet at one layout boundary. The initial edge is
 * bottom, but windows use work_area(), not their own panel coordinates. */
static struct desk_box panel_box(void) { return (struct desk_box){0, height - 54, width, 54}; }
static struct desk_box work_area(void) { return (struct desk_box){0, 0, width, panel_box().y}; }
static struct desk_box language_box(void)
{ return (struct desk_box){width - 102, panel_box().y + 8, 42, 37}; }
static struct desk_box start_box(void) { return (struct desk_box){9, panel_box().y + 1, 153, 51}; }
static struct desk_box menu_box(void) { return (struct desk_box){9, panel_box().y - 273, 313, 260}; }
static struct desk_box task_button(int number)
{ return (struct desk_box){164 + number * min(160, (width - 252) / WINDOWS), panel_box().y + 2,
                           min(156, (width - 252) / WINDOWS - 4), 48}; }

static int active(void)
{
    for (int i = WINDOWS - 1; i >= 0; --i)
        if (win[order[i]].open && !win[order[i]].minimized) return order[i];
    return -1;
}

static bool visible(int id) { return win[id].open && !win[id].minimized; }
#ifdef STYLEOS_OWNED_SHELL_865
/* Kernel-owned cache, never supplied by a user process. Read only between
 * native slices; drawing consumes the cached snapshot, not the scheduler. */
static struct pa_row process_rows[PA_MAX_ROWS], process_next[PA_MAX_ROWS];
static struct pa_reply process_reply;
static bool process_valid, process_refresh, process_was_visible;
enum { PROCESS_LIST, PROCESS_PERFORMANCE, PROCESS_DETAILS, PROCESS_TAB_COUNT };
static unsigned process_tab,process_offset;
static bool process_tab_report;
static unsigned process_poll;
static uint32_t process_selected,process_pending;
static bool process_confirm_drawn,process_dirty;
static const char *process_notice;
static bool process_has_pid(uint32_t pid)
{
    if(!process_valid||!pid)return false;
    for(unsigned i=0;i<process_reply.returned;++i)
        if(process_rows[i].pid==pid&&process_rows[i].type==PR_NATIVE_USER&&
           process_rows[i].state==PR_READY)return true;
    return false;
}
static void process_cancel(void)
{
    if(process_pending){process_pending=0;process_confirm_drawn=false;
        process_notice="Отменено";process_dirty=true;}
}
static void process_select(uint32_t pid)
{
    process_cancel();
    process_selected=process_has_pid(pid)?pid:0;
    process_notice=0;process_dirty=true;
}
static void process_switch_tab(unsigned tab)
{
    if(tab>=PROCESS_TAB_COUNT||tab==process_tab)return;
    process_cancel();process_selected=0;process_notice=0;
    process_tab=tab;process_offset=0;process_dirty=true;process_tab_report=true;
}
static uint64_t process_live_bytes(void)
{
    uint64_t bytes=0;
    if(process_valid)
        for(unsigned i=0;i<process_reply.returned;++i)
            bytes+=process_rows[i].resident_bytes; /* verified 3 pages per row, at most 32 */
    return bytes;
}
static void process_decimal(char out[24], uint64_t value)
{
    char rev[24]; unsigned n=0;
    do { rev[n++]=(char)('0'+value%10u); value/=10u; } while(value);
    for(unsigned i=0;i<n;++i)out[i]=rev[n-i-1];
    out[n]=0;
}
static void process_field(const char *key,uint64_t value)
{
    char buf[24];process_decimal(buf,value);arch_puts(key);arch_puts(buf);
}
static unsigned process_page_size(void)
{
    int n=(win[TASKS].box.h-180)/24;
    return (unsigned)(n>0?n:1);
}
/* UI actions are consumed in the trusted single-CPU shell, outside CPL3.
 * Confirmation binds to a PID, not a recycled row index, and must have been
 * rendered in a previous input batch. Enter/second click cannot auto-arm. */
static void process_arm(void)
{
    if(process_tab!=PROCESS_LIST||process_pending||!process_has_pid(process_selected))return;
    process_pending=process_selected;process_confirm_drawn=false;
    process_notice=0;process_dirty=true;
    arch_puts("STYLEOS PROCESSES ARM");process_field(" pid=",process_pending);arch_puts("\n");
}
static void process_confirm(void)
{
    if(process_tab!=PROCESS_LIST||!process_pending||!process_confirm_drawn)return;
    const uint32_t pid=process_pending;
    process_pending=0;process_confirm_drawn=false;process_selected=0;
    enum ps_status status=s865_native_stop_selected(pid);
    process_notice=status==PS_OK?"Процесс завершён":"Завершение отклонено";
    process_dirty=true;
    arch_puts("STYLEOS PROCESSES STOP");process_field(" pid=",pid);
    process_field(" status=",status);
    process_field(" remaining=",pr_snapshot(pr_global(),0,0,false));arch_puts("\n");
}
static bool process_sample(bool report)
{
    const struct pa_request q={.version=PA_VERSION,.order=PI_BY_PID};
    struct pa_reply next={0};
    enum pa_status st=s865_native_snapshot(&q,process_next,PA_MAX_ROWS,&next);
    bool valid=st==PA_OK&&next.returned<=PA_MAX_ROWS&&next.total==next.returned&&
        next.measurement_flags==(PA_MEASURE_CPU_TICKS|PA_MEASURE_MEMORY_BYTES);
    bool changed=valid!=process_valid;
    if(valid){
        if(next.process_serial!=process_reply.process_serial||
           next.window_generation!=process_reply.window_generation||
           next.returned!=process_reply.returned)changed=true;
        for(unsigned i=0;i<next.returned;++i){
            if(process_next[i].open_windows!=process_rows[i].open_windows||
               process_next[i].group!=process_rows[i].group)changed=true;
            process_rows[i]=process_next[i];
        }
        process_reply=next;
        if(process_offset>=next.returned)process_offset=0;
    }else{
        process_reply=(struct pa_reply){0};process_offset=0;
    }
    process_valid=valid;
    if(process_selected&&!process_has_pid(process_selected)){
        process_cancel();process_selected=0;changed=true;
    }
    if(process_tab_report){
        arch_puts("STYLEOS PROCESSES TAB");
        process_field(" tab=",process_tab);process_field(" valid=",process_valid);
        process_field(" total=",process_reply.returned);
        process_field(" bytes=",process_live_bytes());
        process_field(" flags=",process_reply.measurement_flags);arch_puts("\n");
        process_tab_report=false;
    }
    if(report){
        arch_puts("STYLEOS PROCESSES SNAPSHOT");
        process_field(" valid=",valid);process_field(" total=",process_reply.returned);
        process_field(" flags=",process_reply.measurement_flags);arch_puts("\n");
        if(valid)for(unsigned i=0;i<process_reply.returned;++i){
            const struct pa_row *p=&process_rows[i];
            arch_puts("STYLEOS PROCESSES ROW");
            process_field(" pid=",p->pid);process_field(" group=",p->group);
            process_field(" state=",p->state);process_field(" ticks=",p->cpu_ticks);
            process_field(" bytes=",p->resident_bytes);arch_puts("\n");
        }
        arch_puts("STYLEOS PROCESSES END\n");
    }
    return changed;
}
#endif
static int window_at(int x, int y)
{
    for (int i = WINDOWS - 1; i >= 0; --i) {
        int id = order[i];
        if (visible(id) && contains(win[id].box, x, y)) return id;
    }
    return -1;
}

static struct desk_box drop_target(int x, int y)
{
    if (!transfer.lifted || menu || !win[NOTES].open) return (struct desk_box){0};
    int n = 0;
    for (int i = 0; i < WINDOWS; ++i) if (win[i].open) {
        struct desk_box button = task_button(n++);
        if (i == NOTES && contains(button, x, y)) return button;
    }
    struct desk_box b = win[NOTES].box;
    struct desk_box body = {b.x + 12, b.y + 119, b.w - 24, b.h - 151};
    return window_at(x, y) == NOTES && contains(body, x, y) ? body : (struct desk_box){0};
}
static void front(int id)
{
    unsigned i = 0;
    while (order[i] != id) ++i;
    for (; i + 1 < WINDOWS; ++i) order[i] = order[i + 1];
    order[WINDOWS - 1] = id;
}
static enum desk_session_kind session_kind_for(int id)
{
    if (id == FILES) return DESK_SESSION_EXPLORER;
    if (id == NOTES) return DESK_SESSION_EDITOR;
    if (id == TASKS) return DESK_SESSION_PROCESSES;
    if (id == NOTE_WINDOW) return DESK_SESSION_NOTES;
    return DESK_SESSION_COUNT; /* ABOUT is a shell-owned system window */
}

static void print_pid(const char *prefix, uint32_t pid)
{
    char digits[12];
    unsigned n = 0;
    do { digits[n++] = (char)('0' + pid % 10); pid /= 10; } while (pid);
    char readable[12];
    for (unsigned i = 0; i < n; ++i) readable[i] = digits[n - 1 - i];
    readable[n] = 0;
    arch_puts(prefix);
    arch_puts(readable);
    arch_puts("\n");
}

static void open_window(int id)
{
    bool restore = win[id].minimized, opening = !win[id].open;
    if (opening && session_kind_for(id) != DESK_SESSION_COUNT) {
        uint32_t pid = desk_sessions_start(desk_sessions_global(), session_kind_for(id));
        if (!pid) { arch_puts("STYLEOS PROCESS REFUSED\n"); return; }
        print_pid("STYLEOS PROCESS START pid=", pid);
    }
    win[id].open = true;
    win[id].minimized = false;
    front(id);
#ifdef STYLEOS_OWNED_SHELL_865
    if(id==TASKS)process_refresh=true;
#endif
    if (restore) arch_puts("STYLEOS DESKTOP WINDOW RESTORE\n");
    else if (opening) arch_puts("STYLEOS DESKTOP WINDOW OPEN\n");
}

static void new_draft(int target)
{
    open_window(target);
    if (dirty) { notice = "Сначала сохраните текущий текст"; return; }
#ifdef STYLEOS_LIVE_VOLUME_39
    if(target==NOTES)paged_editor_open=false;
#endif
    document = -1;
    doc_folder = filesystem.mode ? filesystem.folder : current_folder;
    length = caret = 0;
    scroll = 0;
    notice = target == NOTE_WINDOW ? "Новая заметка" :
             (filesystem.mode ? "Новый текстовый файл" : "Новая заметка");
}
static void new_document(void) { new_draft(NOTES); }
static void new_note(void) { new_draft(NOTE_WINDOW); }

static bool note_filename(const char *name)
{
    static const char prefix[] = "Заметка";
    if (!name) return false;
    for (size_t i = 0; i < sizeof(prefix) - 1; ++i)
        if (name[i] != prefix[i]) return false;
    return true;
}

static void open_document(unsigned slot)
{
    const struct sfs_entry *entry = filesystem.mode ?
        sfs_entry_at(&filesystem.disk, slot) : 0;
    int target = entry && note_filename(entry->name) ? NOTE_WINDOW : NOTES;
    open_window(target);
    if ((int)slot == document) return;
    if (dirty) { notice = "Сначала сохраните текущий текст"; return; }
    size_t bytes;
    bool loaded = filesystem.mode ? desk_fs_read_text(&filesystem, slot, incoming, &bytes) :
                                     desk_store_load(&store, slot, incoming, &bytes);
    if (!loaded) { notice = "Не удалось открыть текстовый файл"; return; }
    for (size_t i = 0; i < bytes; ++i) content[i] = incoming[i];
    document = (int)slot;
    doc_folder = filesystem.mode ? filesystem.disk.entries[slot].parent : store.file[slot].folder;
    length = bytes;
    caret = 0;
    scroll = 0;
    notice = "Открыто с диска";
    arch_puts("STYLEOS DESKTOP DOCUMENT OPEN\n");
#if (defined(STYLEOS_TEST_DESKTOP_STORAGE_SERVICE_863) || defined(STYLEOS_OWNED_SHELL_865))
    s863_storage_document(false,(unsigned)document,content,length);
#endif
}

#ifdef STYLEOS_LIVE_VOLUME_39
bool s43_open_editor(const struct po64_row *row)
{
    if(s43_editor_dirty()){
        open_window(NOTES);
        if(active()==NOTES)notice="Сначала сохраните текущий текст";
        return false;
    }
    size_t bytes=0;
    if(!s43_read_text(row,incoming,sizeof incoming,&bytes))return false;
    for(size_t off=0;off<bytes;){
        size_t used=0;uint32_t cp=desk_utf8(incoming+off,bytes-off,&used);
        if(!used||used>bytes-off||(cp<32&&cp!='\r'&&cp!='\n'&&cp!='\t')||
           (cp==0xfffd&&used==1))return false;
        off+=used;
    }
    open_window(NOTES);
    if(active()!=NOTES)return false;
    for(size_t i=0;i<bytes;i++)content[i]=incoming[i];
    unsigned n=s43_name_length(row);
    for(unsigned i=0;i<n;i++)paged_editor_name[i]=(char)row->entry.key.name[i];
    paged_editor_name[n]=0;
    paged_editor_open=true;
    document=-1;doc_folder=0;length=bytes;caret=0;scroll=0;dirty=false;
#ifdef STYLEOS_LIVE_VOLUME_44
    paged_editor_row=*row;
    notice=s44_writable()?"Открыто с диска":"Открыто с диска — только чтение";
#else
    notice="Открыто с диска — только чтение";
#endif
    uint64_t hash=UINT64_C(14695981039346656037);
    for(size_t i=0;i<bytes;i++){hash^=(uint8_t)content[i];hash*=UINT64_C(1099511628211);}
    static const char hex[]="0123456789abcdef";
    arch_puts("SFS43 TEXT OPEN ");
    for(unsigned i=0;i<16;i++)arch_putc(hex[(hash>>(60-4*i))&15u]);
    arch_putc('\n');
    return true;
}
#endif
static void save(void)
{
#ifdef STYLEOS_LIVE_VOLUME_44
    /* Never fall through to the legacy slot backend in this profile. */
    if(active()!=NOTES||!paged_editor_open){notice="Сначала откройте текстовый файл";return;}
    if(!dirty)return;
    if(!s44_save_text(&paged_editor_row,content,length)){
        notice="Не удалось сохранить — текст сохранён в редакторе";
        arch_puts("SFS44 SAVE REFUSED\n");return;
    }
    dirty=false;notice="Сохранено на диске";
    arch_puts("SFS44 SAVE PASS\n");return;
#endif
    if (!data_writable()) { notice = "Диск данных недоступен для записи"; return; }
    int slot = document;
    bool saved;
    if (filesystem.mode)
        saved = active() == NOTE_WINDOW ?
            desk_fs_save_note(&filesystem, doc_folder, &slot, content, length) :
            desk_fs_save_text(&filesystem, doc_folder, &slot, content, length);
    else {
        if (slot < 0) for (int i = 0; i < DESK_DOCS; ++i)
            if (!store.file[i].generation && !store.file[i].blocked) { slot = i; break; }
        if (slot < 0) { notice = "Нет свободных слотов для заметок"; return; }
        saved = desk_store_save(&store, (unsigned)slot, doc_folder, content, length);
    }
    if (!saved) {
        notice = "Не удалось сохранить";
        arch_puts("STYLEOS DESKTOP DOCUMENT SAVE ERROR\n");
        return;
    }
    document = slot;
    dirty = false;
    notice = "Сохранено на диске";
    arch_puts("STYLEOS DESKTOP DOCUMENT SAVE\n");
#if (defined(STYLEOS_TEST_DESKTOP_STORAGE_SERVICE_863) || defined(STYLEOS_OWNED_SHELL_865))
    s863_storage_document(true,(unsigned)document,content,length);
#endif
}

static int text_top(void) { return modern_shell() ? 128 : 96; }
static int text_columns(struct desk_box b) { return max(1, (b.w - 42) / CELL); }
static int text_rows(struct desk_box b) { return max(1, (b.h - text_top() - 28) / LINE); }
static void advance(uint32_t cp, int cols, int *row, int *col)
{
    if (cp == '\n') { ++*row; *col = 0; }
    else if (++*col == cols) { ++*row; *col = 0; }
}

static void position(size_t offset, int cols, int *row, int *col)
{
    *row = *col = 0;
    for (size_t n, off = 0; off < offset; off += n)
        advance(desk_utf8(content + off, offset - off, &n), cols, row, col);
}

static size_t offset_at(int row, int col, int cols)
{
    int r = 0, c = 0;
    size_t off = 0;
    while (off < length) {
        if (r > row || (r == row && c >= col)) break;
        size_t n;
        uint32_t cp = desk_utf8(content + off, length - off, &n);
        if (r == row && (cp == '\n' || c + 1 == cols)) break;
        advance(cp, cols, &r, &c);
        off += n;
    }
    return off;
}

static void edit_scroll(void)
{
    int row, col, visible = text_rows(win[NOTES].box);
    position(caret, text_columns(win[NOTES].box), &row, &col);
    if (row < scroll) scroll = row;
    if (row >= scroll + visible) scroll = row - visible + 1;
}

static bool insert_drop(void)
{
    if (!filesystem.disk.mounted || transfer.generation != filesystem.disk.generation ||
        !drop_target(mx, my).w) return false;
    if (transfer.id == document) {
        open_window(NOTES);
        notice = "Этот файл уже открыт";
        return true;
    }
    size_t count;
    if (!desk_fs_read_text(&filesystem, (unsigned)transfer.id, incoming, &count) || !count) return false;
    open_window(NOTES);
    if (count > sizeof(content) - length) { notice = "Недостаточно места для вставки"; return true; }
    for (size_t i = length; i > caret; --i) content[i - 1 + count] = content[i - 1];
    for (size_t i = 0; i < count; ++i) content[caret + i] = incoming[i];
    caret += count;
    length += count;
    dirty = true;
    notice = "Deep Drop: текст вставлен и ещё не сохранён";
    edit_scroll();
    arch_puts("STYLEOS DESKTOP DEEP DROP TEXT INSERT\n");
    return true;
}

static void replace(size_t lo, size_t hi, uint32_t cp)
{
#ifdef STYLEOS_LIVE_VOLUME_39
    if(active()==NOTES&&paged_editor_open
#ifdef STYLEOS_LIVE_VOLUME_44
       &&!s44_writable()
#endif
       ){
        notice="Файл доступен только для чтения";
        arch_puts("SFS43 EDIT REFUSED\n");return;
    }
#endif
#ifdef STYLEOS_LIVE_VOLUME_44
    if(active()==NOTES&&paged_editor_open){
        desk_text_app_replace(current_draft(),lo,hi,cp,DESK_STYLEFS_TEXT_LIMIT);return;
    }
#endif
    desk_text_app_replace(current_draft(), lo, hi, cp,
                          filesystem.mode ? DESK_STYLEFS_TEXT_LIMIT : DESK_TEXT_LIMIT);
}

static size_t before(size_t p)
{
    if (p) while (--p && ((uint8_t)content[p] & 0xc0) == 0x80) {}
    return p;
}
static size_t after(size_t p)
{
    size_t n;
    (void)desk_utf8(content + p, length - p, &n);
    return p + n;
}

static uint32_t character(unsigned key, bool shift)
{
    static const char normal[] = "\0\0" "1234567890-=\0\0" "qwertyuiop[]\0\0" "asdfghjkl;'`\0\\" "zxcvbnm,./\0\0\0 ";
    static const char upper[] = "\0\0" "!@#$%^&*()_+\0\0" "QWERTYUIOP{}\0\0" "ASDFGHJKL:\"~\0|" "ZXCVBNM<>?\0\0\0 ";
    static const uint16_t ru[0x3a] = {
        [0x10]=0x439,0x446,0x443,0x43a,0x435,0x43d,0x433,0x448,0x449,0x437,0x445,0x44a,
        [0x1e]=0x444,0x44b,0x432,0x430,0x43f,0x440,0x43e,0x43b,0x434,0x436,0x44d,0x451,
        [0x2c]=0x44f,0x447,0x441,0x43c,0x438,0x442,0x44c,0x431,0x44e,
    };
    if (russian && key < sizeof(ru) / sizeof(*ru) && ru[key])
        return shift ? (ru[key] == 0x451 ? 0x401 : ru[key] - 32u) : ru[key];
    if (russian && key == 0x35) return shift ? ',' : '.';
    return key < sizeof(normal) - 1 ? (uint8_t)(shift ? upper[key] : normal[key]) : 0;
}

static int protected_file_entry(void)
{
    return win[NOTES].open ? (document >= 0 ? document : (int)doc_folder) : -1;
}

static void toggle_language(void)
{
    russian = !russian;
    arch_puts(russian ? "STYLEOS DESKTOP INPUT RU\n" : "STYLEOS DESKTOP INPUT EN\n");
}

static void restart_shell(void);
#ifdef STYLEOS_OWNED_SHELL_865
static bool queue_window_action(unsigned key,int id,int dx,int dy);
#ifndef STYLEOS_NATIVE_PINNED_CATALOG
static uint32_t shell_native_target;
#endif
#endif

static bool keyboard(unsigned key, unsigned mods)
{
    last_file_click.at = 0;
    current_file_click.id = -1;
#ifdef STYLEOS_OWNED_SHELL_865
#ifndef STYLEOS_NATIVE_PINNED_CATALOG
    /* Until native close/activation IPC exists, do not close or switch
     * a legacy window behind a focused native one. Keep native focus on
     * the global language chord, which belongs to the input service. */
    if(key==DESK_KEY_LANGUAGE||key==0x40){toggle_language();return true;}
    if(native_input.focus&&key==0x3eu&&mods==DESK_ALT){
        if(s865_shell_submit_command(0x30a))shell_native_target=native_input.focus;
        return false;
    }
    if(native_input.focus&&key==0x0fu&&mods==DESK_ALT){
        (void)s865_shell_submit_command(0x30b);return false;
    }
#endif
    /* Global shell shortcuts retain precedence; selecting a legacy window
     * clears native keyboard focus. The driver currently emits key presses. */
    const bool system_key=(key>=0x3bu&&key<=0x44u)||key==0x57u||key==0x58u||
                          key==DESK_KEY_LANGUAGE||key==0x15bu||
                          (key==0x01u&&mods==(DESK_CTRL|DESK_SHIFT))||
                          (key==0x0fu&&mods==DESK_ALT);
    if(ndb_route_key((struct nwc_service *)nwc_global(),&native_input,
                     width,work_area().h,key,mods,menu||system_key))return false;
#endif
#ifdef STYLEOS_LIVE_VOLUME_46
    if(active()!=FILES||menu)s46_cancel();
    else if(s46_input(key,mods,character(key,(mods&DESK_SHIFT)!=0),
                      paged_editor_open?paged_editor_row.entry.target:0)){
        ++filesystem.revision;return true;
    }
#endif
#ifdef STYLEOS_OWNED_SHELL_865
    if(active()==TASKS&&!menu&&!mods){
        if(key==0x14b||key==0x14d){
            process_switch_tab((process_tab+PROCESS_TAB_COUNT+(key==0x14b?PROCESS_TAB_COUNT-1:1))%PROCESS_TAB_COUNT);
            return true;
        }
        if(process_tab!=PROCESS_LIST&&(key==0x153||key==0x1c||key==0x148||key==0x150))return true;
        if(key==0x01&&process_pending){process_cancel();return true;}
        if(key==0x153){process_arm();return true;}
        if(key==0x1c&&process_pending){process_confirm();return true;}
        if(key==0x148||key==0x150){
            unsigned idx=process_offset;
            for(unsigned i=0;i<process_reply.returned;++i)
                if(process_rows[i].pid==process_selected){
                    idx=key==0x148?(i?i-1:0):i+1;break;
                }
            if(idx<process_reply.returned){
                process_select(process_rows[idx].pid);
                if(idx<process_offset||idx>=process_offset+process_page_size())
                    process_offset=idx;
            }
            return true;
        }
    }
    process_cancel();
#ifndef STYLEOS_NATIVE_PINNED_CATALOG
    /* Common accelerators reserve the system path before app input.
     * Escape in the Ctrl+Shift chord is not a plain Escape. */
    if(key==0x01u&&mods==(DESK_CTRL|DESK_SHIFT)){
        if(s865_shell_submit_command(0x42))return false;
    }
    if(key==0x41u&&!mods){
        (void)s865_shell_submit_command(0x30c);return false;
    }
    if(key==0x0fu&&mods==DESK_ALT){
        if(s865_shell_submit_command(0x309))return false;
    }
    if(key==0x3eu&&mods==DESK_ALT){
        int id=active();
        if(id>=0)(void)queue_window_action(0x101u,id,0,0);
        return false;
    }
#endif
#endif
#if defined(STYLEOS_OWNED_SHELL_865) && defined(STYLEOS_NATIVE_PINNED_CATALOG)
    if (key == 0x43 && !mods) { (void)s865_native_catalog_request(); return false; }
#elif defined(STYLEOS_TEST_DESKTOP_COLD_TASKS_863) && !defined(STYLEOS_OWNED_SHELL_865)
    if (key == 0x43 && !mods) { (void)s863_request_tasks(); return false; }
#endif
    if (key == 0x44) {
#ifdef STYLEOS_OWNED_SHELL_865
        if(s865_shell_submit_refresh())return false;
#endif
        restart_shell();
#if defined(STYLEOS_OWNED_SHELL_865) && defined(STYLEOS_NATIVE_PINNED_CATALOG)
        s865_native_request_shell_gap();
#endif
        return true;
    } /* F10, preserve app state and PIDs */
    if (key == 0x42) {
#ifdef STYLEOS_OWNED_SHELL_865
        if(s865_shell_submit_command(0x42))return false;
#endif
        open_window(TASKS); return true;
    } /* F8 processes */
    if (key == 0x3f) { open_window(NOTE_WINDOW); return true; } /* F5 Notes */
    if (key == 0x01 && transfer.id >= 0) { transfer.id = -1; transfer.lifted = false; return true; }
    if (key == 0x40 || key == DESK_KEY_LANGUAGE) { toggle_language(); return true; } /* F6 or Alt+Shift */
    if (key == 0x01 && menu) { menu = false; return true; }
    if (key == 0x15b || key == 0x3e) {
#ifdef STYLEOS_OWNED_SHELL_865
        if(s865_shell_submit_command(0x3e))return false;
#endif
        menu = !menu; return true;
    } /* Super or F4 */
#ifdef STYLEOS_OWNED_SHELL_865
    if(active()==TASKS&&!menu&&!mods&&(key==0x149||key==0x151)){
        unsigned step=process_page_size();
        if(key==0x149)process_offset=process_offset>=step?process_offset-step:0;
        else if(process_offset+step<process_reply.returned)process_offset+=step;
        process_refresh=true;
        return true;
    }
#endif
#ifdef STYLEOS_LIVE_VOLUME_39
    if (active()==FILES&&!menu&&!mods&&s41_root_view()&&s42_navigate(key)){
        ++filesystem.revision;return true;
    }
#endif
    if (filesystem.mode && active() == FILES && key != 0x3d && !menu) {
        int item = desk_fs_key(&filesystem, win[FILES].box, key, mods,
                               character(key, (mods & DESK_SHIFT) != 0), protected_file_entry());
        if (item >= 0) open_document((unsigned)item);
        return true;
    }
    if (key == 0x3c) { open_window(FILES); return true; }
    if (key == 0x3d) { open_window(NOTES); return true; }
    if (active() != NOTES && active() != NOTE_WINDOW) return false;
    if (mods & DESK_CTRL) {
        if (key == 0x1f) save();
        else if (key == 0x31) {
            if (active() == NOTE_WINDOW) new_note();
            else new_document();
        }
        else if (key == 0x18 && modern_shell()) open_window(FILES);
        else return false;
    } else if (mods & DESK_ALT) return false;
    else if (key == 0x0e) replace(before(caret), caret, 0);
    else if (key == 0x153) replace(caret, after(caret), 0);
    else if (key == 0x14b) caret = before(caret);
    else if (key == 0x14d) caret = after(caret);
    else if (key == 0x148 || key == 0x150 || key == 0x147 || key == 0x14f) {
        int r, c, cols = text_columns(win[NOTES].box);
        position(caret, cols, &r, &c);
        if (key == 0x148) { if (r) caret = offset_at(r - 1, c, cols); }
        else if (key == 0x150) caret = offset_at(r + 1, c, cols);
        else caret = offset_at(r, key == 0x147 ? 0 : cols, cols);
    } else {
        uint32_t cp = key == 0x1c || key == 0x11c ? '\n' : character(key, (mods & DESK_SHIFT) != 0);
        if (!cp) return false;
        replace(caret, caret, cp);
    }
    edit_scroll();
    return true;
}

/* Directories are real locations in the bounded data format. Document rows
 * come from the mounted store, not a screenshot or a fixed item list. */
enum { BACK = -3, DIR_DOCUMENTS = -2, DIR_PROJECTS = -1 };
static unsigned file_rows(int *items)
{
    unsigned count = 0;
    if (current_folder == DESK_ROOT) { items[count++] = DIR_DOCUMENTS; items[count++] = DIR_PROJECTS; }
    else items[count++] = BACK;
    for (unsigned n = 0; n < DESK_DOCS; ++n)
        if (store.file[n].generation && store.file[n].folder == current_folder) items[count++] = (int)n;
    return count;
}
static void show_folder(unsigned where)
{
    if (filesystem.mode) desk_fs_shortcut(&filesystem, where);
    else current_folder = where;
    open_window(FILES);
}
static void pick_file(int item)
{
    if (item == BACK) show_folder(DESK_ROOT);
    else if (item == DIR_DOCUMENTS) show_folder(DESK_DOCUMENTS);
    else if (item == DIR_PROJECTS) show_folder(DESK_PROJECTS);
    else open_document((unsigned)item);
}

static const struct desk_box desktop_shortcuts[] = {{28,38,80,84}, {28,142,80,84}, {28,246,80,84}};
static const int menu_rows[] = {18, 62, 103, 163, 209};

static struct desk_box shortcut_box(unsigned index)
{
    struct desk_box a = desktop_shortcuts[index];
    if (modern_shell()) { a.x = 18; a.w = 112; }
    return a;
}

static struct desk_box file_highlight(struct desk_box b, unsigned row)
{
    return (struct desk_box){b.x + 12, b.y + TITLE + 12 + (int)row * ROW, b.w - 24, ROW};
}

static struct desk_box menu_highlight(struct desk_box menu, unsigned row)
{
    return (struct desk_box){menu.x + 10, menu.y + menu_rows[row] - 1, menu.w - 20, 38};
}

struct visual {
    struct desk_box boxes[WINDOWS], hot[6];
    bool shown[WINDOWS], open[WINDOWS];
    int z[WINDOWS], top, x, y;
    bool menu, russian, writable;
    unsigned folder, filesystem_revision;
    uint32_t generations[DESK_DOCS], stylefs_generation;
};

static void take_visual(struct visual *v)
{
    *v = (struct visual){.top = active(), .x = mx, .y = my, .menu = menu,
                        .russian = russian, .folder = current_folder, .writable = data_writable(),
                        .filesystem_revision = filesystem.revision,
                        .stylefs_generation = filesystem.disk.generation};
    for (int id = 0; id < WINDOWS; ++id) {
        v->boxes[id] = win[id].box;
        v->shown[id] = visible(id);
        v->open[id] = win[id].open;
        v->z[order[id]] = id;
    }
    for (unsigned i = 0; i < DESK_DOCS; ++i) v->generations[i] = store.file[i].generation;

    for (unsigned i = 0; i < sizeof(desktop_shortcuts) / sizeof(*desktop_shortcuts); ++i)
        if (contains(shortcut_box(i), mx, my)) v->hot[0] = shortcut_box(i);
    if (!menu && contains(start_box(), mx, my)) v->hot[1] = start_box();
    if (!filesystem.mode && (!modern_shell() || store.writable) && v->top == FILES) {
        const struct desk_box b = win[FILES].box;
        int items[DESK_DOCS + 2];
        unsigned n = file_rows(items);
        for (unsigned i = 0; i < n; ++i) {
            const struct desk_box row = file_highlight(b, i);
            if (row.y + 42 > b.y + b.h - 20) break;
            if (contains(row, mx, my)) { v->hot[2] = row; break; }
        }
    }
    if (menu) for (unsigned i = 1; i < 4; ++i) {
        struct desk_box row = menu_highlight(menu_box(), i);
        if (contains(row, mx, my)) v->hot[3] = row;
    }
    v->hot[4] = drop_target(mx, my);
    if (contains(language_box(), mx, my)) v->hot[5] = language_box();
}

static void invalidate(struct desk_damage *d, const struct visual *old,
                       const struct visual *next, bool editor, bool clock)
{
    bool task_state = false, panel = old->top != next->top;
    for (int id = 0; id < WINDOWS; ++id) {
        bool visibility = old->shown[id] != next->shown[id];
        if (visibility || old->open[id] != next->open[id]) task_state = true;
        if (old->open[id] != next->open[id]) panel = true;
        bool focus = old->top != next->top && (old->top == id || next->top == id);
        if (visibility || focus || old->z[id] != next->z[id] ||
            !desk_box_equal(old->boxes[id], next->boxes[id])) {
            if (old->shown[id]) desk_damage_add(d, old->boxes[id]);
            if (next->shown[id]) desk_damage_add(d, next->boxes[id]);
        }
    }
    bool files = old->folder != next->folder || old->writable != next->writable ||
                 old->filesystem_revision != next->filesystem_revision;
    for (unsigned i = 0; i < DESK_DOCS; ++i)
        if (old->generations[i] != next->generations[i]) files = true;
    if (files && next->shown[FILES]) desk_damage_add(d, next->boxes[FILES]);
    if ((editor || old->stylefs_generation != next->stylefs_generation ||
         old->writable != next->writable) && next->shown[NOTES]) desk_damage_add(d, next->boxes[NOTES]);
    if ((editor || old->stylefs_generation != next->stylefs_generation ||
         old->writable != next->writable) && next->shown[NOTE_WINDOW])
        desk_damage_add(d, next->boxes[NOTE_WINDOW]);
    if (task_state && next->shown[TASKS]) desk_damage_add(d, next->boxes[TASKS]);
    if (old->writable != next->writable && next->shown[ABOUT]) desk_damage_add(d, next->boxes[ABOUT]);

    if (panel) desk_damage_add(d, panel_box());
    else if (clock || old->russian != next->russian)
        desk_damage_add(d, (struct desk_box){width - 102, panel_box().y + 8, 102, 37});
    if (old->menu != next->menu) {
        desk_damage_add(d, start_box());
        desk_damage_add(d, menu_box());
    }
    for (unsigned i = 0; i < sizeof(old->hot) / sizeof(*old->hot); ++i) {
        if (!desk_box_equal(old->hot[i], next->hot[i])) {
            desk_damage_add(d, old->hot[i]);
            desk_damage_add(d, next->hot[i]);
        }
    }
}
static void maximize(int id)
{
    struct window *w = &win[id];
    if (w->maximized) w->box = w->previous;
    else { w->previous = w->box; w->box = work_area(); }
    w->maximized = !w->maximized;
    if (id == NOTES || id == NOTE_WINDOW) edit_scroll();
}

/* Title commands preserve the existing dirty-document guard and the
 * independent application sessions. A controller never receives a pointer. */
static void close_legacy_window(int id)
{
    struct window *w=&win[id];
    if ((id == NOTES || id == NOTE_WINDOW) && dirty)
        notice = "Перед закрытием сохраните текст";
    else {
        if ((id == NOTES || id == NOTE_WINDOW) && modern_shell()) {
#ifdef STYLEOS_LIVE_VOLUME_39
            if(id==NOTES)paged_editor_open=false;
#endif
            document = -1; length = caret = 0; scroll = 0; notice = "";
        }
        w->open=false;
        if(session_kind_for(id)!=DESK_SESSION_COUNT)
            (void)desk_sessions_stop(desk_sessions_global(),session_kind_for(id));
        arch_puts("STYLEOS DESKTOP WINDOW CLOSE\n");
    }
}
#ifdef STYLEOS_OWNED_SHELL_865
static struct {int id,dx,dy;} shell_target={-1,0,0};
static bool queue_window_action(unsigned key,int id,int dx,int dy)
{
    if(id<0||id>=WINDOWS||!s865_shell_submit_command(key))return false;
    shell_target.id=id;shell_target.dx=dx;shell_target.dy=dy;return true;
}
static void apply_window_action(unsigned action)
{
    int id=shell_target.id;
    int dx=shell_target.dx,dy=shell_target.dy;
    shell_target.id=-1;
    if(id<0||id>=WINDOWS||!win[id].open)return;
    struct window *w=&win[id];
    if(action==8){open_window(id);return;}
    if(!visible(id))return;
    front(id);
    if(action==4)close_legacy_window(id);
    else if(action==5)maximize(id);
    else if(action==6){w->minimized=true;arch_puts("STYLEOS DESKTOP WINDOW MINIMIZE\n");}
    else if(action==7&&!w->maximized&&mouse_down){
        drag=id;drag_x=dx;drag_y=dy;drag_moved=false;
    }
}
#endif
static void shell_reboot_request(void)
{
    if(desk_sessions_has_unsaved(desk_sessions_global())){
        open_window(desk_sessions_notes_unsaved(desk_sessions_global())?NOTE_WINDOW:NOTES);
        notice="Перед перезагрузкой сохраните текст";
    }else{arch_puts("STYLEOS DESKTOP REBOOT\n");desk_reboot();}
}
static void click(int x, int y)
{
#ifdef STYLEOS_LIVE_VOLUME_46
    s46_cancel();
#endif
#ifdef STYLEOS_OWNED_SHELL_865
    if(!menu&&window_at(x,y)==TASKS){
        struct desk_box b=win[TASKS].box;
        if(y>=b.y+48&&y<b.y+76&&x>=b.x+10&&x<b.x+b.w-10){
            int dx=x-b.x;
            front(TASKS);
            process_switch_tab(dx<b.w*25/100?PROCESS_LIST:
                               dx<b.w*70/100?PROCESS_PERFORMANCE:PROCESS_DETAILS);
            return;
        }
        if(process_tab==PROCESS_LIST&&y>=b.y+b.h-28&&y<b.y+b.h-4&&x>=b.x+b.w-160&&x<b.x+b.w-10){
            front(TASKS);
            if(process_pending)process_confirm();else process_arm();
            return;
        }
        if(process_tab==PROCESS_LIST&&y>=b.y+137&&y<b.y+b.h-48){
            unsigned i=process_offset+(unsigned)(y-b.y-137)/24;
            front(TASKS);
            process_select(i<process_reply.returned&&i<process_offset+process_page_size()?process_rows[i].pid:0);
            return;
        }
    }
    process_cancel();
#endif
    if (contains(start_box(), x, y)) {
#ifdef STYLEOS_OWNED_SHELL_865
        if(s865_shell_submit_command(0x3e))return;
#endif
        menu = !menu; return;
    }
    if (contains(language_box(), x, y)) {
#ifdef STYLEOS_OWNED_SHELL_865
        if(s865_shell_submit_command(0x306))return;
#endif
        menu = false; toggle_language(); return;
    }
    if (menu) {
        struct desk_box m = menu_box();
        if (contains(m, x, y)) {
            int d = y - m.y;
#ifdef STYLEOS_OWNED_SHELL_865
            unsigned command=d>=13&&d<58?0x301:d>=58&&d<98?0x302:
                d>=98&&d<140?0x303:d>=156&&d<201?0x304:0x307;
            if(s865_shell_submit_command(command))return;
#endif
            menu = false;
            if (d >= 13 && d < 58) show_folder(DESK_ROOT);
            else if (d < 98 && d >= 58) open_window(ABOUT);
            else if (d < 140 && d >= 98) open_window(TASKS);
            else if (d >= 156 && d < 201) shell_reboot_request();
            return;
        }
        menu = false;
    }
    if (contains(panel_box(), x, y)) {
        int n = 0;
        for (int i = 0; i < WINDOWS; ++i) if (win[i].open) {
            if (contains(task_button(n++), x, y)) {
#ifdef STYLEOS_OWNED_SHELL_865
                if(queue_window_action(active()==i?0x200u:0x201u,i,0,0))return;
#endif
                if (active() == i) { win[i].minimized = true; arch_puts("STYLEOS DESKTOP WINDOW MINIMIZE\n"); }
                else open_window(i);
                break;
            }
        }
        return;
    }
    for (int n = WINDOWS - 1; n >= 0; --n) {
        int id = order[n];
        struct window *w = &win[id];
        struct desk_box b = w->box;
        if (!w->open || w->minimized || !contains(b, x, y)) continue;
        int dx = x - b.x, dy = y - b.y, right = b.w - dx;
#ifdef STYLEOS_OWNED_SHELL_865
        if(dy<TITLE&&queue_window_action(0x100u+(unsigned)min(right,126),id,dx,dy))return;
#endif
        front(id);
        if (dy < TITLE) {
            if (right < 42) {
                close_legacy_window(id);
            } else if (right < 84) maximize(id);
            else if (right < 126) {
                w->minimized = true;
                arch_puts("STYLEOS DESKTOP WINDOW MINIMIZE\n");
            } else if (!w->maximized) { drag = id; drag_x = dx; drag_y = dy; drag_moved = false; }
#ifdef STYLEOS_LIVE_VOLUME_39
        } else if(id==FILES&&s41_root_view()){
            if(desk_fs_paged_click(s41_root_view(),b,x,y))++filesystem.revision;
#endif
        } else if (id == FILES && filesystem.mode) {
            int item = desk_fs_click(&filesystem, b, x, y, protected_file_entry());
            if (item == DESK_FS_NEW_TEXT) new_document();
            else if (item >= 0) open_document((unsigned)item);
        } else if (id == FILES && (!modern_shell() || store.writable) && dy >= TITLE + 12 && dy < b.h - 25) {
            int items[DESK_DOCS + 2];
            unsigned count = file_rows(items), at = (unsigned)(dy - TITLE - 12) / ROW;
            if (at < count) pick_file(items[at]);
        } else if (id == NOTES || id == NOTE_WINDOW) {
            if (dy >= 52 && dy < 83 && dx >= 12 && dx < 88) new_document();
            else if (dy >= 52 && dy < 83 && modern_shell() && dx >= 94 && dx < 192) open_window(FILES);
            else if (dy >= 52 && dy < 83 && ((!modern_shell() && dx >= 94 && dx < 195) ||
                                                (modern_shell() && dx >= 198 && dx < 304))) save();
            else if (dy >= text_top() && dy < b.h - 29 && dx >= 18) {
                caret = offset_at(scroll + (dy - text_top()) / LINE, (dx - 18) / CELL, text_columns(b));
                edit_scroll();
            }
        }
        return;
    }
    if (contains(shortcut_box(0), x, y)) {
#ifdef STYLEOS_OWNED_SHELL_865
        if(s865_shell_submit_command(0x301))return;
#endif
        show_folder(DESK_ROOT);
    } else if (contains(shortcut_box(1), x, y)) {
#ifdef STYLEOS_OWNED_SHELL_865
        if(s865_shell_submit_command(0x305))return;
#endif
        open_window(NOTES);
    } else if (contains(shortcut_box(2), x, y)) {
#ifdef STYLEOS_OWNED_SHELL_865
        if(s865_shell_submit_command(0x308))return;
#endif
        show_folder(DESK_PROJECTS);
    }
}

static void accent(struct desk_box b)
{
    desk_fill(b.x + 5, b.y, b.w - 10, 2, GOLD);
    desk_fill(b.x + 2, b.y + 2, b.w - 4, 1, GOLD);
}

static void draw_without_data(struct desk_box b)
{
    int right = b.x + b.w - 16, side = min(b.w / 3, 222), left = b.x + side;
    desk_text(b.x + 16, b.y + 53, right, "Мой компьютер", LIGHT);
    desk_fill(b.x + 1, b.y + 83, b.w - 2, 1, STROKE);
    desk_fill(left, b.y + 84, 1, b.h - 85, STROKE);
    desk_text(b.x + 16, b.y + 95, left - 6, "Быстрый доступ", MUTED);
    desk_text(b.x + 16, b.y + 129, left - 6, "Документы", 0x748297);
    desk_text(b.x + 16, b.y + 160, left - 6, "Обновления", 0x748297);
    desk_text(left + 15, b.y + 94, right, "Диски", LIGHT);
    desk_icon(left + 15, b.y + 135, 29, DESK_DRIVE, GOLD);
    desk_text(left + 56, b.y + 135, right, data_description(), LIGHT);
    desk_text(left + 15, b.y + 172, right, "Проверьте диск StyleFS на вторичном IDE 0.", MUTED);
    desk_text(left + 15, b.y + 200, right, "Носитель не форматировался.", MUTED);
}

static void draw_window(int id, int top)
{
    struct desk_editor_draft *previous_draft = rendering_draft;
    if (id == NOTES) rendering_draft = &desk_sessions_global()->editor;
    else if (id == NOTE_WINDOW) rendering_draft = &desk_sessions_global()->notes;
    else rendering_draft = 0;
    struct desk_box b = win[id].box;
    desk_round(b, WINDOW);
    desk_outline(b, STROKE);
    if (id == top) accent(b);
    if (modern_shell()) desk_icon(b.x + 15, b.y + 9, 24, app_icon(id), LIGHT);
    desk_text(b.x + (modern_shell() ? 47 : 16), b.y + 11, b.x + b.w - 128, app_name(id), LIGHT);
    int r = b.x + b.w, y = b.y + 19;
    desk_fill(b.x + 8, b.y + TITLE, b.w - 16, 1, STROKE);
    desk_fill(r - 115, y + 4, 13, 2, LIGHT);
    desk_fill(r - 73, y - 4, 13, 2, LIGHT);
    desk_fill(r - 73, y + 7, 13, 2, LIGHT);
    desk_fill(r - 73, y - 4, 2, 12, LIGHT);
    desk_fill(r - 62, y - 4, 2, 12, LIGHT);
    desk_line(r - 31, y - 3, r - 20, y + 8, LIGHT);
    desk_line(r - 20, y - 3, r - 31, y + 8, LIGHT);

#ifdef STYLEOS_LIVE_VOLUME_39
    if (id == FILES && s41_root_view()) desk_fs_paged_root_draw(s41_root_view(),b);
    else
#endif
    if (id == FILES && modern_shell() && !data_available()) draw_without_data(b);
    else if (id == FILES && filesystem.mode) desk_fs_draw(&filesystem, b);
    else if (id == FILES) {
        int items[DESK_DOCS + 2];
        unsigned count = file_rows(items);
        for (unsigned i = 0; i < count; ++i) {
            int at = items[i], row = b.y + TITLE + 12 + (int)i * ROW;
            if (row + 42 > b.y + b.h - 20) break;
            bool hover = id == top && contains(file_highlight(b, i), mx, my);
            if (hover) desk_round((struct desk_box){b.x + 12, row + 1, b.w - 24, ROW - 2}, 0x253143);
            desk_icon(b.x + 26, row + 1, 38, at < 0 ? DESK_FOLDER : DESK_PAPER, at < 0 ? FOLDER : LIGHT);
            const char *label = at == BACK ? "Назад" : at == DIR_DOCUMENTS ? folders[DESK_DOCUMENTS] :
                                at == DIR_PROJECTS ? folders[DESK_PROJECTS] : files[at];
            desk_text(b.x + 84, row + 11, r - 16, label, LIGHT);
        }
        int foot = b.y + b.h - 24;
        if (!store.writable) desk_text(b.x + 18, foot, r - 10, "Диск данных не доступен для записи", MUTED);
        else if (modern_shell()) desk_text(b.x + 18, foot, r - 10, "Старый диск заметок. Обновления недоступны.", MUTED);
        else if (current_folder != DESK_ROOT) desk_text(b.x + 18, foot, r - 10, folders[current_folder], MUTED);
    } else if (id == NOTES || id == NOTE_WINDOW) {
        desk_round((struct desk_box){b.x + 12, b.y + 52, 76, 31}, 0x253143);
        desk_text(b.x + 22, b.y + 58, b.x + 84, "Новый", LIGHT);
        if (modern_shell()) {
            desk_round((struct desk_box){b.x + 94, b.y + 52, 98, 31}, 0x253143);
            desk_text(b.x + 103, b.y + 58, b.x + 191, "Открыть", LIGHT);
            desk_round((struct desk_box){b.x + 198, b.y + 52, 106, 31}, 0x4e3c2a);
            desk_text(b.x + 207, b.y + 58, b.x + 303, "Сохранить", LIGHT);
        } else {
            desk_round((struct desk_box){b.x + 94, b.y + 52, 101, 31}, 0x4e3c2a);
            desk_text(b.x + 102, b.y + 58, b.x + 194, "Сохранить", LIGHT);
        }
        const struct sfs_entry *entry = filesystem.mode && document >= 0 ?
            sfs_entry_at(&filesystem.disk, (unsigned)document) : 0;
        const char *label = filesystem.mode ? (entry ? entry->name : "Без имени") :
                                              (document < 0 ? "Без имени" : files[document]);
#ifdef STYLEOS_LIVE_VOLUME_39
        if(id==NOTES&&paged_editor_open)label=paged_editor_name;
#endif
        if (modern_shell()) {
            desk_fill(b.x + 10, b.y + 89, b.w - 20, 1, STROKE);
            desk_round((struct desk_box){b.x + 12, b.y + 94, 236, 29}, 0x4e3c2a);
            int end = desk_text(b.x + 22, b.y + 98, b.x + 229, label, LIGHT);
            if (dirty) desk_text(end + 6, b.y + 98, b.x + 244, "*", GOLD);
            desk_fill(b.x + 12, b.y + 121, 236, 2, GOLD);
        } else {
            desk_text(b.x + 210, b.y + 58, r - 15, label, MUTED);
            desk_round((struct desk_box){b.x + 12, b.y + 90, b.w - 24, b.h - 120}, 0x111a25);
        }
        int tr = 0, tc = 0, ncols = text_columns(b), nrows = text_rows(b);
        for (size_t off = 0; off < length;) {
            size_t n;
            uint32_t cp = desk_utf8(content + off, length - off, &n);
            if (tr >= scroll + nrows) break;
            if (tr >= scroll && cp >= 32)
                (void)desk_glyph(b.x + 18 + tc * CELL, b.y + text_top() + (tr - scroll) * LINE, r - 15, cp, LIGHT);
            advance(cp, ncols, &tr, &tc);
            off += n;
        }
        if (id == top) {
            position(caret, ncols, &tr, &tc);
            if (tr >= scroll && tr < scroll + nrows)
                desk_fill(b.x + 18 + tc * CELL, b.y + text_top() + 2 + (tr - scroll) * LINE, 1, 17, GOLD);
        }
        if (modern_shell()) {
            desk_fill(b.x + 10, b.y + b.h - 27, b.w - 20, 1, STROKE);
            desk_text(r - 65, b.y + b.h - 23, r - 9, "UTF-8", MUTED);
        }
        desk_text(b.x + 14, b.y + b.h - 24, r - (modern_shell() ? 77 : 12), notice, MUTED);
    } else if (id == TASKS) {
#ifdef STYLEOS_OWNED_SHELL_865
        const int t1=b.x+b.w*25/100,t2=b.x+b.w*70/100;
        const int tx[4]={b.x+10,t1,t2,r-10};
        const char *const tabs[PROCESS_TAB_COUNT]={"Процессы","Производительность","Подробности"};
        for(unsigned i=0;i<PROCESS_TAB_COUNT;++i){
            if(i==process_tab)desk_fill(tx[i],b.y+48,tx[i+1]-tx[i],27,0x344154);
            desk_text(tx[i]+4,b.y+53,tx[i+1]-3,tabs[i],i==process_tab?GOLD:LIGHT);
        }
        desk_fill(b.x+10,b.y+76,b.w-20,1,STROKE);
        if(!process_valid){
            desk_text(b.x+12,b.y+88,r-10,"Снимок недоступен",MUTED);
        }else if(process_tab==PROCESS_PERFORMANCE){
            char buf[24];
            desk_text(b.x+12,b.y+88,r-12,"Пользовательские процессы",GOLD);
            process_decimal(buf,process_reply.returned);
            desk_text(b.x+12,b.y+114,b.x+b.w*70/100,"Зарегистрировано",LIGHT);
            desk_text(b.x+b.w*70/100,b.y+114,r-12,buf,LIGHT);
            process_decimal(buf,process_live_bytes()/1024u);
            desk_text(b.x+12,b.y+138,b.x+b.w*70/100,"Страницы процессов, КиБ",LIGHT);
            desk_text(b.x+b.w*70/100,b.y+138,r-12,buf,LIGHT);
            desk_text(b.x+12,b.y+162,r-12,"CPU %: нет измерения времени",MUTED);
            desk_text(b.x+12,b.y+186,r-12,"RAM системы: нет измерения",MUTED);
            desk_text(b.x+12,b.y+210,r-12,"Диск / сеть: нет измерения",MUTED);
        }else{
            const int a=b.x+b.w*32/100,c=b.x+b.w*47/100,d=b.x+b.w*65/100,e=b.x+b.w*82/100;
            char nbuf[24];
            int pos=desk_text(b.x+12,b.y+84,r-12,"Прил. ",MUTED);
            process_decimal(nbuf,process_reply.applications);pos=desk_text(pos,b.y+84,r-12,nbuf,LIGHT);
            pos=desk_text(pos,b.y+84,r-12,"  Фон. ",MUTED);
            process_decimal(nbuf,process_reply.background);pos=desk_text(pos,b.y+84,r-12,nbuf,LIGHT);
            pos=desk_text(pos,b.y+84,r-12,"  Сист. ",MUTED);
            process_decimal(nbuf,process_reply.system);desk_text(pos,b.y+84,r-12,nbuf,LIGHT);
            if(!process_reply.returned){
                desk_text(b.x+12,b.y+113,r-12,"Нет зарегистрированных процессов",LIGHT);
            }else{
                desk_text(b.x+12,b.y+111,a-3,"Имя",MUTED);
                desk_text(a,b.y+111,c-3,"PID",MUTED);
                desk_text(c,b.y+111,d-3,process_tab==PROCESS_LIST?"Группа":"PPID",MUTED);
                desk_text(d,b.y+111,e-3,process_tab==PROCESS_LIST?"Статус":"Окна",MUTED);
                desk_text(e,b.y+111,r-8,process_tab==PROCESS_LIST?"КиБ":"Тики",MUTED);
                unsigned end=process_offset+process_page_size();
                if(end>process_reply.returned)end=process_reply.returned;
                for(unsigned i=process_offset;i<end;++i){
                    const struct pa_row *p=&process_rows[i];
                    int y=b.y+137+(int)(i-process_offset)*24;
                    char buf[24];
                    if(process_tab==PROCESS_LIST&&p->pid==process_selected)
                        desk_fill(b.x+9,y-2,b.w-18,23,0x344154);
                    desk_text(b.x+12,y,a-3,p->name,LIGHT);
                    process_decimal(buf,p->pid);desk_text(a,y,c-3,buf,LIGHT);
                    if(process_tab==PROCESS_LIST){
                        desk_text(c,y,d-3,p->group==PI_APPLICATIONS?"Прил.":p->group==PI_BACKGROUND?"Фон":"Сист.",LIGHT);
                        desk_text(d,y,e-3,p->state==PR_READY?"Готов":p->state==PR_WAITING?"Ждёт":"Вып.",LIGHT);
                        process_decimal(buf,p->resident_bytes/1024u);
                    }else{
                        process_decimal(buf,p->parent_pid);desk_text(c,y,d-3,buf,LIGHT);
                        process_decimal(buf,p->open_windows);desk_text(d,y,e-3,buf,LIGHT);
                        process_decimal(buf,p->cpu_ticks);
                    }
                    desk_text(e,y,r-8,buf,MUTED);
                }
            }
        }
        desk_text(b.x+12,b.y+b.h-48,r-10,process_tab==PROCESS_DETAILS?"Тики: принятые IRQ, не время CPU":"CPU %: нет данных; память: страницы",MUTED);
        if(process_pending){
            char buf[24];process_decimal(buf,process_pending);
            int after=desk_text(b.x+12,b.y+b.h-25,r-165,"Завершить PID ",GOLD);
            desk_text(after,b.y+b.h-25,r-165,buf,GOLD);
        }else desk_text(b.x+12,b.y+b.h-25,r-165,process_notice?process_notice:"F8 / PgUp / PgDn",MUTED);
        if(process_tab==PROCESS_LIST&&(process_pending||process_has_pid(process_selected))){
            desk_round((struct desk_box){r-160,b.y+b.h-28,150,24},0x76572f);
            desk_text(r-154,b.y+b.h-25,r-12,process_pending?"Подтвердить":"Завершить",LIGHT);
        }
#else
        const struct desk_sessions *sessions = desk_sessions_global();
        desk_text(b.x + 18, b.y + 55, r - 15, "Процессы StyleOS", GOLD);
        desk_text(b.x + 18, b.y + 87, b.x + 251, "Оболочка", LIGHT);
        char pid_text[32];
        unsigned row = 1;
        const uint32_t shell_pid = sessions->shell_pid;
        unsigned number = shell_pid, n = 0;
        char rev[12];
        do { rev[n++] = (char)('0' + number % 10); number /= 10; } while (number);
        pid_text[0] = 'P'; pid_text[1] = 'I'; pid_text[2] = 'D'; pid_text[3] = ' ';
        for (unsigned j = 0; j < n; ++j) pid_text[4 + j] = rev[n - j - 1];
        pid_text[4 + n] = 0;
        desk_text(b.x + 257, b.y + 87, r - 15, pid_text, MUTED);
        for (int kind = 0; kind < DESK_SESSION_COUNT; ++kind) {
            uint32_t pid = desk_sessions_pid(sessions, (enum desk_session_kind)kind);
            if (!pid) continue;
            int y = b.y + 87 + (int)(row++) * 28;
            desk_text(b.x + 18, y, b.x + 251, desk_sessions_name((enum desk_session_kind)kind), LIGHT);
            n = 0; number = pid;
            do { rev[n++] = (char)('0' + number % 10); number /= 10; } while (number);
            for (unsigned j = 0; j < n; ++j) pid_text[4 + j] = rev[n - j - 1];
            pid_text[4 + n] = 0;
            desk_text(b.x + 257, y, r - 15, pid_text, MUTED);
        }
        desk_text(b.x + 18, b.y + b.h - 27, r - 15, "F10: обновить оболочку, сохранить приложения", MUTED);
#endif
    } else {
#ifdef STYLEOS_DESKTOP_UPDATE
        desk_text(b.x + 18, b.y + 60, r - 15, "StyleOS Desktop v0.7", LIGHT);
#else
        desk_text(b.x + 18, b.y + 60, r - 15, "StyleOS Desktop v0.6", LIGHT);
#endif
        desk_text(b.x + 18, b.y + 90, r - 15, "x86-64 UEFI", MUTED);
        desk_text(b.x + 18, b.y + 120, r - 15, (input_flags & 1) ? "Клавиатура PS/2 готова" : "Нет клавиатуры PS/2", MUTED);
        desk_text(b.x + 18, b.y + 149, r - 15, (input_flags & 2) ? "Мышь PS/2 готова" : "Нет мыши PS/2", MUTED);
        desk_text(b.x + 18, b.y + 178, r - 15, data_description(), MUTED);
#ifdef STYLEOS_DESKTOP_UPDATE
        desk_text(b.x + 18, b.y + 207, r - 15, desk_update_status(), LIGHT);
#endif
    }
    rendering_draft = previous_draft;
}

static void draw(void)
{
    desk_wallpaper();
    for (unsigned i = 0; i < 3; ++i) {
        struct desk_box b = shortcut_box(i);
        if (!desk_clip_visible(b)) continue;
        if (contains(b, mx, my)) desk_round(b, 0x2b374b);
        desk_icon(b.x + (modern_shell() ? 27 : 15), b.y + 3, 50,
                  i == 2 ? DESK_FOLDER : app_icon((int)i), i == 2 ? FOLDER : LIGHT);
        const char *label = i == 2 ? folders[DESK_PROJECTS] :
                            i == 1 && modern_shell() ? "Редактор" : app_name((int)i);
        desk_text(b.x + 9, b.y + 57, b.x + b.w, label, LIGHT);
    }
    int top = active();
    for (unsigned i = 0; i < WINDOWS; ++i)
        if (visible(order[i]) && desk_clip_visible(win[order[i]].box)) draw_window(order[i], top);

    /* Only authenticated kernel-owned tiles, never direct user pointers. */
#ifdef STYLEOS_OWNED_SHELL_865
    (void)ndb_paint_top(nwc_global(),width,work_area().h,native_input.raised,native_plot,native_fill,0);
#else
    (void)ndb_paint_rects(nwc_global(),width,work_area().h,native_plot,native_fill,0);
#endif

    struct desk_box panel = panel_box(), start = start_box();
    desk_fill(panel.x, panel.y, panel.w, panel.h, PANEL);
    desk_fill(panel.x, panel.y, panel.w, 1, 0x2c394d);
    if (menu || contains(start, mx, my)) { desk_round(start, 0x1a2638); desk_outline(start, 0x414e61); }
    desk_icon(24, panel.y + 8, 32, DESK_LOGO, LIGHT);
    desk_text(66, panel.y + 16, 163, "StyleOS", LIGHT);
    int n = 0;
    for (int i = 0; i < WINDOWS; ++i) if (win[i].open) {
        struct desk_box b = task_button(n++);
        desk_round(b, i == top ? 0x202c3e : 0x172231);
        if (i == top) accent(b);
        desk_icon(b.x + 12, b.y + 12, 23, app_icon(i), LIGHT);
        desk_text(b.x + 45, b.y + 14, b.x + b.w - 3,
                  modern_shell() && i == NOTES ? "Редактор" : app_name(i), LIGHT);
    }
    struct desk_box lang = language_box();
    desk_round(lang, contains(lang, mx, my) ? 0x34455e : 0x202c40);
    desk_text(lang.x + 9, panel.y + 16, width - 61, russian ? "RU" : "EN", LIGHT);
    desk_text(width - 59, panel.y + 16, width - 4, clock_text, LIGHT);
    if (menu && desk_clip_visible(menu_box())) {
        struct desk_box m = menu_box();
        desk_round(m, 0x1d2838);
        desk_outline(m, STROKE);
        static const uint8_t symbols[] = {DESK_FILE_MANAGER, DESK_INFO, DESK_TASK, DESK_RESTART, DESK_POWER};
        static const char *const labels[] = {"Файлы", "О системе", processes_name, "Перезагрузить", "Выключить"};
        desk_fill(m.x + 17, m.y + 147, m.w - 34, 1, STROKE);
        for (int i = 0; i < 5; ++i) {
            int y = m.y + menu_rows[i];
            struct desk_box b = menu_highlight(m, (unsigned)i);
            if (i == 0 || (i != 4 && contains(b, mx, my))) desk_round(b, 0x76572f);
            unsigned glyph = i == 0 ? app_icon(FILES) : i == 2 ? app_icon(TASKS) : symbols[i];
            desk_icon(m.x + 27, y + 6, 24, glyph, i == 4 ? 0x7b8595 : LIGHT);
            desk_text(m.x + 76, y + 8, m.x + m.w - 10,
                      i == 0 ? app_name(FILES) : labels[i], i == 4 ? 0x7b8595 : LIGHT);
        }
    }
    struct desk_box target = drop_target(mx, my);
    if (target.w) desk_outline(target, GOLD);
    /* Draw a visible cursor last without using any firmware callbacks. */
    for (int i = 0; i < 14; ++i) desk_fill(mx, my + i, i / 2 + 2, 1, 0x0b1018);
    for (int i = 1; i < 12; ++i) desk_fill(mx + 1, my + i, i / 2 + 1, 1, 0xf3f0e9);
    desk_line(mx + 4, my + 10, mx + 8, my + 16, LIGHT);
}

static bool mouse(const struct desk_event *event)
{
    mx = clamp(mx + event->dx, 0, width - 1);
    my = clamp(my + event->dy, 0, height - 1);
    bool down = (event->buttons & 1) != 0;
    bool pressed = down && !mouse_down;
#ifdef STYLEOS_OWNED_SHELL_865
    if(ndb_route_pointer((struct nwc_service *)nwc_global(),&native_input,
                         width,work_area().h,mx,my,event->buttons,event->modifiers,
                         pressed,!down&&mouse_down,menu||drag>=0||transfer.id>=0)){
        mouse_down=down;
        current_file_click.id=-1;last_file_click.at=0;
        return false;
    }
#endif
    bool inserted = false;
    if (current_file_click.id >= 0 &&
        (mx - current_file_click.x > 5 || current_file_click.x - mx > 5 ||
         my - current_file_click.y > 5 || current_file_click.y - my > 5))
        current_file_click.id = -1;
    if (pressed) {
        bool in_files = filesystem.mode && !menu && !contains(panel_box(), mx, my) && window_at(mx, my) == FILES;
        int item = in_files ? desk_fs_hit_item(&filesystem, win[FILES].box, mx, my) : -1;
        uint64_t now = item >= 0 ? desk_pointer_clock() : 0;
        second_file_click = item >= 0 && last_file_click.id == item &&
            last_file_click.revision == filesystem.revision && desk_pointer_near(last_file_click.at, now) &&
            last_file_click.x >= mx - 5 && last_file_click.x <= mx + 5 &&
            last_file_click.y >= my - 5 && last_file_click.y <= my + 5;
        last_file_click.at = 0;
        current_file_click = (struct file_click){.id = item, .x = mx, .y = my, .at = now};
        int source = item >= 0 ? desk_fs_drag_source(&filesystem, win[FILES].box, mx, my) : -1;
        transfer.id = source;
        transfer.generation = filesystem.disk.generation;
        transfer.x = mx;
        transfer.y = my;
        transfer.lifted = false;
        click(mx, my);
        current_file_click.revision = filesystem.revision;
    }
    else if (down && drag >= 0) {
        struct desk_box *b = &win[drag].box;
        struct desk_box a = work_area();
        int x = clamp(mx - drag_x, a.x, a.x + a.w - b->w);
        int y = clamp(my - drag_y, a.y, a.y + a.h - b->h);
        if (x != b->x || y != b->y) drag_moved = true;
        b->x = x;
        b->y = y;
    } else if (!down && drag >= 0) {
        if (drag_moved) arch_puts("STYLEOS DESKTOP WINDOW DRAG\n");
        drag = -1;
    } else if (transfer.id >= 0) {
        if (down && (mx - transfer.x > 5 || transfer.x - mx > 5 ||
                     my - transfer.y > 5 || transfer.y - my > 5)) transfer.lifted = true;
        if (!down) {
            if (transfer.lifted) inserted = insert_drop();
            transfer.id = -1;
            transfer.lifted = false;
        }
    }
    if (!down && mouse_down) {
        if (!inserted && current_file_click.id >= 0 && !menu && window_at(mx, my) == FILES &&
            current_file_click.revision == filesystem.revision &&
            desk_fs_hit_item(&filesystem, win[FILES].box, mx, my) == current_file_click.id) {
            uint64_t now = desk_pointer_clock();
            if (second_file_click && desk_pointer_near(current_file_click.at, now)) {
                int opened = desk_fs_open_item(&filesystem, current_file_click.id);
                if (opened >= 0) open_document((unsigned)opened);
                arch_puts("STYLEOS DESKTOP FILE DOUBLE CLICK\n");
                inserted = true;
            } else {
                last_file_click = current_file_click;
                last_file_click.at = now;
            }
        }
        current_file_click.id = -1;
    }
    mouse_down = down;
    return pressed || inserted;
}

static void initial_windows(void)
{
    int h = work_area().h;
    win[FILES].box = (struct desk_box){width * 688 / 1536, h * 128 / 804, width * 756 / 1536, max(350, h * 419 / 804)};
    win[NOTES].box = (struct desk_box){width * 405 / 1536, h * 175 / 804, width * 900 / 1536, h * 488 / 804};
    win[NOTE_WINDOW].box = (struct desk_box){width * 435 / 1536, h * 200 / 804,
                                             width * 830 / 1536, h * 445 / 804};
    win[TASKS].box = (struct desk_box){width / 3, h / 6, min(width / 2, 510), min(h * 4 / 5, 316)};
    win[ABOUT].box = (struct desk_box){width / 3, h / 5, min(width / 2, 490), min(h * 4 / 5, 238)};
    if (modern_shell()) {
        int x = width * 182 / 1536, y = h * 54 / 804;
        win[FILES].box = (struct desk_box){x, y, width - x - 46, h - y - 49};
        win[NOTES].box = (struct desk_box){x + 24, y + 27, width - x - 76, h - y - 75};
        win[NOTE_WINDOW].box = (struct desk_box){x + 48, y + 43, width - x - 125, h - y - 116};
    }
    win[FILES].open = true;
}

/* Recreate only compositor-owned view state. App PIDs, unsaved document text,
 * file-system and open app sessions are never reinitialized on F10. */
static void restart_shell(void)
{
#if defined(STYLEOS_DESKTOP_V0) || (defined(STYLEOS_TEST_DESKTOP_INTERACTIVE_863) || defined(STYLEOS_OWNED_SHELL_865))
    desktop_kernel_boundary864();
#endif
    struct desk_sessions *sessions = desk_sessions_global();
    uint32_t new_pid = desk_sessions_restart_shell(sessions);
    if (!new_pid) { arch_puts("STYLEOS SHELL RESTART REFUSED\n"); return; }
    bool opened[WINDOWS];
    for (int i = 0; i < WINDOWS; ++i) {
        opened[i] = win[i].open;
        win[i] = (struct window){0};
    }
    order[0] = ABOUT; order[1] = TASKS; order[2] = NOTE_WINDOW;
    order[3] = NOTES; order[4] = FILES;
    menu = false;
    drag = -1;
    mouse_down = false;
    second_file_click = false;
    last_file_click.at = 0;
    current_file_click.id = -1;
    transfer.id = -1;
    transfer.lifted = false;
    initial_windows();
    for (int i = 0; i < WINDOWS; ++i) win[i].open = opened[i];
#ifdef STYLEOS_OWNED_SHELL_865
    process_cancel();process_selected=0;process_notice=0;process_tab=PROCESS_LIST;
    process_offset=0;process_tab_report=false;process_dirty=true;
#endif
    shell_refreshed = true;
#ifdef STYLEOS_OWNED_SHELL_865
    native_input=(struct ndb_input){0};
#endif
    print_pid("STYLEOS SHELL RESTART pid=", new_pid);
#if defined(STYLEOS_DESKTOP_V0) || (defined(STYLEOS_TEST_DESKTOP_INTERACTIVE_863) || defined(STYLEOS_OWNED_SHELL_865))
    desktop_kernel_boundary864();
#endif
    arch_puts("STYLEOS DESKTOP SHELL RESTART\n");
}

static bool update_clock(void)
{
    unsigned h, m;
    if (!desk_clock(&h, &m)) return false;
    char values[5] = {(char)('0' + h / 10), (char)('0' + h % 10), ':', (char)('0' + m / 10), (char)('0' + m % 10)};
    bool change = false;
    for (unsigned i = 0; i < sizeof(values); ++i) {
        if (clock_text[i] != values[i]) change = true;
        clock_text[i] = values[i];
    }
    return change;
}

#if (defined(STYLEOS_TEST_DESKTOP_FRAME_852) || defined(STYLEOS_OWNED_SHELL_865))
/* Reuse the real shell renderer and initial window geometry. This diagnostic
 * does not enter the event loop, probe devices, mount storage or create PIDs. */
size_t desk_first_frame_852(const struct desktop_surface *surface)
{
    if (!desktop_surface_valid(surface) || surface->width < 800 || surface->height < 600)
        return 0;
    width = (int)surface->width;
    height = (int)surface->height;
    mx = width / 2;
    my = height * 3 / 4;
    initial_windows();
    desk_draw_init(surface);
    return desk_present(&(struct desk_damage){.all = true}, draw);
}
#endif

#ifdef STYLEOS_TEST_DESKTOP_INPUT_853
/* Fixed diagnostic motion away from hover/drag targets. No UI actions. */
size_t desk_cursor_853(const struct desk_event *event)
{
    if (!event || !event->mouse || event->buttons || event->modifiers ||
        width != 1280 || height != 800 || mx != 640 || my != 600 ||
        event->dx != 20 || event->dy != 0) return 0;
    struct desk_damage damage = {0};
    desk_damage_add(&damage, (struct desk_box){mx, my, CURSOR_W, CURSOR_H});
    mx = clamp(mx + event->dx, 0, width - 1);
    desk_damage_add(&damage, (struct desk_box){mx, my, CURSOR_W, CURSOR_H});
    return desk_present(&damage, draw);
}
#endif

/* One bounded desktop iteration. Native process dispatch belongs to
 * the kernel supervisor; no scheduler pump inside the view callback. */
struct shell_cycle { unsigned tick; bool cursor_proven; };
static void desktop_shell_step(void *opaque)
{
    struct shell_cycle *cycle=opaque;
#ifdef STYLEOS_OWNED_SHELL_865
    if(s865_shell_take_refresh())restart_shell();
    if(s865_shell_take_redraw())shell_refreshed=true;
    unsigned shell_action=s865_shell_take_command();
    if(shell_action==2){menu=!menu;shell_refreshed=true;}
    else if(shell_action==3){open_window(TASKS);shell_refreshed=true;}
    else if(shell_action>=4&&shell_action<=8){apply_window_action(shell_action);shell_refreshed=true;}
    else if(shell_action>=9&&shell_action<=20){
        menu=false;
        if(shell_action==9)show_folder(DESK_ROOT);
        else if(shell_action==10)open_window(ABOUT);
        else if(shell_action==11)open_window(TASKS);
        else if(shell_action==12)shell_reboot_request();
        else if(shell_action==13)open_window(NOTES);
        else if(shell_action==14)toggle_language();
        else if(shell_action==16)show_folder(DESK_PROJECTS);
        else if(shell_action==17){
            if(!ndb_cycle_focus(nwc_global(),&native_input,width,work_area().h)){
                int current=active();
                for(int n=WINDOWS-1;n>=0;--n){
                    int id=order[n];
                    if(id!=current&&visible(id)){front(id);break;}
                }
            }
        }
#ifndef STYLEOS_NATIVE_PINNED_CATALOG
        else if(shell_action==18){
            enum nwc_status result=ndb_request_close((struct nwc_service *)nwc_global(),&native_input,
                width,work_area().h,shell_native_target);
            shell_native_target=0;
            arch_puts(result==NWC_OK?"STYLEOS NATIVE CLOSE REQUESTED\n":"STYLEOS NATIVE CLOSE REFUSED\n");
        }else if(shell_action==19){
            (void)ndb_cycle_focus(nwc_global(),&native_input,width,work_area().h);
        }else if(shell_action==20){
            if(!s865_launch_native_sample())arch_puts("STYLEOS TEA69 LAUNCH REFUSED\n");
        }
#endif
        shell_refreshed=true;
    }
#endif
    struct desk_event e;
#if (defined(STYLEOS_TEST_DESKTOP_INTERACTIVE_863) || defined(STYLEOS_OWNED_SHELL_865))
    s863_shell_guard();
    struct desk_event observed863={0};size_t writes863=0;unsigned batch863=0;
#endif
    struct visual old, next;
    bool captured = false, editor = false, clock = false;
    for (unsigned i = 0; i < 32
#ifdef STYLEOS_OWNED_SHELL_865
         && !shell_action && !s865_shell_command_waiting()
#endif
         && desk_input_next(&e); ++i) {
        if (!captured) { take_visual(&old); captured = true; }
#if (defined(STYLEOS_TEST_DESKTOP_INTERACTIVE_863) || defined(STYLEOS_OWNED_SHELL_865))
        observed863=e;batch863++;
#endif
        if (e.mouse) editor |= mouse(&e) &&
                      (active() == NOTES || active() == NOTE_WINDOW);
        else editor |= keyboard(e.key, e.modifiers) &&
                       (active() == NOTES || active() == NOTE_WINDOW);
    }
    if (++cycle->tick == 20000) { cycle->tick = 0; clock = update_clock(); }
#if defined(STYLEOS_DESKTOP_V0) || (defined(STYLEOS_TEST_DESKTOP_INTERACTIVE_863) || defined(STYLEOS_OWNED_SHELL_865))
    if(captured||clock)desktop_kernel_boundary864();
#endif
    bool imported = false;
#ifdef STYLEOS_DESKTOP_UPDATE
    if (desk_import_active()) imported = desk_import_poll(&filesystem);
#endif
    struct desk_damage damage = {0};
#ifdef STYLEOS_OWNED_SHELL_865
    unsigned native_changes=ndb_track_top(nwc_global(),&native_view,width,work_area().h,native_input.raised,&damage);
#else
    unsigned native_changes = ndb_track(nwc_global(), &native_view,
                                        width, work_area().h, &damage);
#endif
    bool process_changed=false;
#ifdef STYLEOS_OWNED_SHELL_865
    const bool process_shown=visible(TASKS);
    bool periodic=false;
    if(++process_poll==128u){process_poll=0;periodic=true;}
    if(!process_shown||active()!=TASKS||menu)process_cancel();
    if(process_shown&&(process_refresh||process_dirty||!process_was_visible||
        (periodic&&(!process_valid||pr_global()->change_serial!=process_reply.process_serial)))){
        process_changed=process_sample(process_refresh)||process_refresh||process_dirty||!process_was_visible;
        if(process_changed)desk_damage_add(&damage,win[TASKS].box);
    }
    process_refresh=false;process_dirty=false;process_was_visible=process_shown;
#endif
    if (captured || clock || imported || shell_refreshed || native_changes || process_changed) {
        if (shell_refreshed) { damage.all = true; shell_refreshed = false; }
        bool cursor_only = false;
        if (captured) {
            take_visual(&next);
            invalidate(&damage, &old, &next, editor, clock);
            if (old.x != next.x || old.y != next.y) {
                cursor_only = !damage.all && !damage.count;
                desk_damage_add(&damage, (struct desk_box){old.x, old.y, CURSOR_W, CURSOR_H});
                desk_damage_add(&damage, (struct desk_box){next.x, next.y, CURSOR_W, CURSOR_H});
            }
        } else if(!process_changed||clock) desk_damage_add(&damage, (struct desk_box){width - 96, panel_box().y + 14, 96, 24});
        if (imported && visible(FILES)) {
            desk_damage_add(&damage, win[FILES].box);
            cursor_only = false;
        }
        size_t writes = desk_present(&damage, draw);
#ifdef STYLEOS_OWNED_SHELL_865
        if(process_pending&&active()==TASKS&&!menu&&writes)process_confirm_drawn=true;
        if(shell_action&&writes)arch_puts("STYLEOS SHELL ACTION RENDERED\n");
#endif
#if (defined(STYLEOS_TEST_DESKTOP_INTERACTIVE_863) || defined(STYLEOS_OWNED_SHELL_865))
        writes863=writes;
#endif
        if (!cycle->cursor_proven && cursor_only && writes && writes <= 2u * CURSOR_W * CURSOR_H) {
            arch_puts("STYLEOS DESKTOP CURSOR DAMAGE PASS\n");
            cycle->cursor_proven = true;
        }
    }
#if (defined(STYLEOS_TEST_DESKTOP_INTERACTIVE_863) || defined(STYLEOS_OWNED_SHELL_865))
    if(captured)s863_shell_observe(&observed863,(unsigned)mx,(unsigned)my,(unsigned)active(),russian,desk_sessions_global()->shell_restarts,note_length863(),note_hash863(),note_dirty863(),note_session863(),writes863,input_flags,batch863);
#endif
}
 
_Noreturn void desktop_v0_run(const struct desktop_surface *surface)
{
#if defined(STYLEOS_DESKTOP_V0) || (defined(STYLEOS_TEST_DESKTOP_INTERACTIVE_863) || defined(STYLEOS_OWNED_SHELL_865))
    desktop_kernel_boundary864();
    arch_puts("STYLEOS DESKTOP RUNTIME BOUNDARY PASS\n");
#endif
#if (defined(STYLEOS_TEST_DESKTOP_INTERACTIVE_863) || defined(STYLEOS_OWNED_SHELL_865))
    s863_shell_guard();
#endif
    width = (int)surface->width;
    height = (int)surface->height;
    if (width < 800 || height < 600) {
        arch_puts("STYLEOS DESKTOP ERROR mode smaller than 800x600\n");
        arch_halt();
    }
    mx = width / 2;
    my = height * 3 / 4;
    native_view=(struct ndb_tracker){0};
#ifdef STYLEOS_OWNED_SHELL_865
    native_input=(struct ndb_input){0};
#endif
    desk_sessions_init(desk_sessions_global());
    desk_draw_init(surface);
    data_sectors = desk_ata_probe();
    desk_fs_mount(&filesystem, data_sectors);
    if (data_sectors == 4096) desk_store_mount(&store);
#ifdef STYLEOS_DESKTOP_UPDATE
    arch_puts(!data_sectors ? "STYLEOS DESKTOP DATA ABSENT\n" :
              filesystem.disk.mounted ? "STYLEOS DESKTOP DATA STYLEFS\n" :
              store.writable ? "STYLEOS DESKTOP DATA LEGACY\n" : "STYLEOS DESKTOP DATA UNRECOGNIZED\n");
#endif
    arch_puts(data_available() ? "STYLEOS DESKTOP DATA MOUNT\n" : "STYLEOS DESKTOP DATA UNAVAILABLE\n");
    input_flags = desk_input_init();
    desk_pointer_clock_init();
    arch_puts(input_flags & 1 ? "STYLEOS DESKTOP KEYBOARD PASS\n" : "STYLEOS DESKTOP KEYBOARD UNAVAILABLE\n");
    arch_puts(input_flags & 2 ? "STYLEOS DESKTOP MOUSE PASS\n" : "STYLEOS DESKTOP MOUSE UNAVAILABLE\n");
    initial_windows();
    print_pid("STYLEOS PROCESS START pid=",
              desk_sessions_start(desk_sessions_global(), DESK_SESSION_EXPLORER));
    (void)update_clock();
#ifdef STYLEOS_DESKTOP_UPDATE
    size_t painted = desk_present(&(struct desk_damage){.all = true}, draw);
    if ((input_flags & 3) == 3 && painted) desk_update_mark_ready();
#else
#if (defined(STYLEOS_TEST_DESKTOP_INTERACTIVE_863) || defined(STYLEOS_OWNED_SHELL_865))
    size_t initial_writes863=desk_present(&(struct desk_damage){.all = true}, draw);
#else
    (void)desk_present(&(struct desk_damage){.all = true}, draw);
#endif
#endif
    arch_puts("STYLEOS DESKTOP READY\n");
#if (defined(STYLEOS_TEST_DESKTOP_INTERACTIVE_863) || defined(STYLEOS_OWNED_SHELL_865))
    s863_shell_observe(0,(unsigned)mx,(unsigned)my,(unsigned)active(),russian,desk_sessions_global()->shell_restarts,note_length863(),note_hash863(),note_dirty863(),note_session863(),initial_writes863,input_flags,0);
#endif
    struct shell_cycle cycle={0};
#ifdef STYLEOS_OWNED_SHELL_865
    s865_native_supervise(desktop_shell_step,&cycle);
#else
    for (;;) {
        desktop_shell_step(&cycle);
#ifdef STYLEOS_TEST_DESKTOP_PUMP_863
        s863_shell_pump();
#endif
        for (unsigned i = 0; i < 64; ++i) __asm__ volatile("pause");
    }
#endif
}
