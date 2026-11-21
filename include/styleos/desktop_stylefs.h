#ifndef STYLEOS_DESKTOP_STYLEFS_H
#define STYLEOS_DESKTOP_STYLEFS_H

#include <styleos/desktop_v0.h>
#include <styleos/stylefs.h>

enum { DESK_FS_NONE = -1, DESK_FS_NEW_TEXT = -2 };
enum { DESK_STYLEFS_TEXT_LIMIT = 64 * 1024 };

struct desk_fs {
    struct sfs_volume disk;
    struct sfs_workspace work;
    unsigned folder, first, revision, name_edit, name_length;
    unsigned history[SFS_ENTRIES], history_at, history_count;
    int selected, clip, delete_id;
    uint32_t delete_generation;
#ifdef STYLEOS_DESKTOP_UPDATE
    int update_id;
    uint32_t update_generation;
#endif
    bool mode, computer, cut, grid, searching;
    bool update_view;
    char query[SFS_NAME_MAX + 1];
    unsigned query_length;
    char name[SFS_NAME_MAX + 1];
    const char *status;
};

void desk_fs_mount(struct desk_fs *, uint32_t sectors);
void desk_fs_shortcut(struct desk_fs *, unsigned target);
int desk_fs_key(struct desk_fs *, struct desk_box, unsigned key, unsigned mods, uint32_t cp, int protected_entry);
int desk_fs_click(struct desk_fs *, struct desk_box, int x, int y, int protected_entry);
int desk_fs_hit_item(const struct desk_fs *, struct desk_box, int x, int y);
int desk_fs_open_item(struct desk_fs *, int item);
int desk_fs_drag_source(const struct desk_fs *, struct desk_box, int x, int y);
void desk_fs_draw(const struct desk_fs *, struct desk_box);
bool desk_fs_read_text(struct desk_fs *, unsigned id, char *data, size_t *size);
bool desk_fs_save_text(struct desk_fs *, unsigned folder, int *id, const char *data, size_t length);
bool desk_fs_save_note(struct desk_fs *, unsigned folder, int *id, const char *data, size_t length);

#endif
