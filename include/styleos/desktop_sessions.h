#ifndef STYLEOS_DESKTOP_SESSIONS_H
#define STYLEOS_DESKTOP_SESSIONS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* App IDs are stable within this guest ABI; they are not OS PIDs. */
enum desk_session_kind {
    DESK_SESSION_EXPLORER,
    DESK_SESSION_EDITOR,
    DESK_SESSION_NOTES,
    DESK_SESSION_DOCUMENTS,
    DESK_SESSION_PROCESSES,
    DESK_SESSION_COUNT
};

enum { DESK_SESSION_TEXT_BYTES = 64 * 1024 };

struct desk_session_record {
    uint32_t pid;
    bool running;
};

/* Stored outside the desktop shell translation unit. This survives a shell
 * UI restart and is eventually supplied by native .tea app processes. */
struct desk_editor_draft {
    char content[DESK_SESSION_TEXT_BYTES];
    size_t length;
    size_t caret;
    int scroll;
    int document;
    unsigned doc_folder;
    bool dirty;
    const char *notice;
};

struct desk_sessions {
    uint32_t next_pid;
    uint32_t shell_pid;
    uint32_t shell_restarts;
    struct desk_session_record app[DESK_SESSION_COUNT];
    struct desk_editor_draft editor;
};

/* The singleton is owned by the runtime, not the window/compositor module. */
struct desk_sessions *desk_sessions_global(void);
void desk_sessions_init(struct desk_sessions *sessions);
uint32_t desk_sessions_start(struct desk_sessions *sessions, enum desk_session_kind kind);
bool desk_sessions_stop(struct desk_sessions *sessions, enum desk_session_kind kind);
uint32_t desk_sessions_pid(const struct desk_sessions *sessions, enum desk_session_kind kind);
uint32_t desk_sessions_restart_shell(struct desk_sessions *sessions);
const char *desk_sessions_name(enum desk_session_kind kind);

#endif
