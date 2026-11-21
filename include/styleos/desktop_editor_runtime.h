#ifndef STYLEOS_DESKTOP_EDITOR_RUNTIME_H
#define STYLEOS_DESKTOP_EDITOR_RUNTIME_H

#include <styleos/desktop_stylefs.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Editor state that survives any shell-only restart.
 * This does not claim EL0 process isolation: the runtime is the first
 * separation boundary, while a real independent app loader remains pending. */
struct desk_editor_runtime {
    char content[DESK_STYLEFS_TEXT_LIMIT];
    char incoming[DESK_STYLEFS_TEXT_LIMIT];
    size_t length, caret;
    unsigned folder;
    int document, scroll;
    bool dirty;
    uint32_t session_id;
};

struct desk_editor_runtime *desk_editor_runtime_open(void);

#endif
