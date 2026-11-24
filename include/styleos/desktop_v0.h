#ifndef STYLEOS_DESKTOP_V0_H
#define STYLEOS_DESKTOP_V0_H

#include <styleos/desktop_display.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct desk_box { int x, y, w, h; };
enum { DESK_DAMAGE_CAPACITY = 24 };
struct desk_damage {
    struct desk_box boxes[DESK_DAMAGE_CAPACITY];
    unsigned count;
    bool all;
};

void desk_draw_init(const struct desktop_surface *surface);
bool desk_box_equal(struct desk_box a, struct desk_box b);
bool desk_clip_visible(struct desk_box box);
void desk_damage_add(struct desk_damage *damage, struct desk_box box);
/* Complete each affected strip in RAM before writing that strip to GOP. */
size_t desk_present(const struct desk_damage *damage, void (*draw_scene)(void));
void desk_wallpaper(void);
void desk_fill(int x, int y, int w, int h, uint32_t rgb);
void desk_round(struct desk_box b, uint32_t rgb);
void desk_outline(struct desk_box b, uint32_t rgb);
void desk_line(int x, int y, int ex, int ey, uint32_t rgb);
int desk_text(int x, int y, int right, const char *string, uint32_t rgb);
int desk_glyph(int x, int y, int right, uint32_t cp, uint32_t rgb);
uint32_t desk_utf8(const char *s, size_t remaining, size_t *used);
void desk_icon(int x, int y, int size, unsigned icon, uint32_t rgb);
enum { DESK_FOLDER, DESK_PAPER, DESK_LOGO, DESK_INFO, DESK_POWER, DESK_RESTART,
       DESK_TASK, DESK_FILE_MANAGER, DESK_APP_SETTINGS, DESK_APP_EXPLORER,
       DESK_APP_EDITOR, DESK_APP_NOTES, DESK_APP_PROCESSES, DESK_APP_DOCUMENTS,
       DESK_DRIVE };

enum { DESK_SHIFT = 1, DESK_CTRL = 2, DESK_ALT = 4, DESK_KEY_LANGUAGE = 0x200 };
struct desk_event {
    int dx, dy;
    uint16_t key; /* keyboard translated set 1, extended keys have bit 8 */
    uint8_t buttons, modifiers;
    bool mouse;
};
unsigned desk_input_init(void);
bool desk_input_next(struct desk_event *p);
bool desk_clock(unsigned *hour, unsigned *minute);
void desk_pointer_clock_init(void);
uint64_t desk_pointer_clock(void);
bool desk_pointer_near(uint64_t earlier, uint64_t later);
void desk_reboot(void);

/* Secondary IDE master only. Writable capacity must be separately admitted. */
uint32_t desk_ata_probe(void);
bool desk_ata_read(uint32_t lba, uint8_t sector[512]);
bool desk_ata_write(uint32_t lba, const uint8_t sector[512]);
bool desk_ata_flush(void);
/* Optional secondary IDE slave. Never auto-format unknown media. */
uint32_t desk_ata_probe_data2(void);
bool desk_ata_read_data2(uint32_t lba, uint8_t sector[512]);
bool desk_ata_write_data2(uint32_t lba, const uint8_t sector[512]);
bool desk_ata_flush_data2(void);

enum { DESK_DOCS = 4, DESK_TEXT_LIMIT = 4096 };
enum { DESK_ROOT, DESK_DOCUMENTS, DESK_PROJECTS };
struct desk_record { uint32_t generation, size; uint8_t folder, active; bool blocked; };
struct desk_store { struct desk_record file[DESK_DOCS]; bool writable; };
void desk_store_mount(struct desk_store *s);
bool desk_store_load(const struct desk_store *s, unsigned slot, char *text, size_t *length);
bool desk_store_save(struct desk_store *s, unsigned slot, unsigned folder,
                     const char *text, size_t length);

#ifdef STYLEOS_TEST_DESKTOP_FRAME_852
/* First frame only: no storage, input, clock or user-process admission. */
size_t desk_first_frame_852(const struct desktop_surface *surface);
#endif
#ifdef STYLEOS_TEST_DESKTOP_INPUT_853
size_t desk_cursor_853(const struct desk_event *event);
#endif
_Noreturn void desktop_v0_run(const struct desktop_surface *surface);

#endif
