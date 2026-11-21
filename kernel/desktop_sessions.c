#include <styleos/desktop_sessions.h>

static struct desk_sessions runtime_sessions;

struct desk_sessions *desk_sessions_global(void)
{
    return &runtime_sessions;
}

/* PID 0 is invalid. No guest PID may be silently reused on exhaustion. */
static uint32_t allocate_pid(struct desk_sessions *s)
{
    if (!s || s->next_pid == 0 || s->next_pid == UINT32_MAX)
        return 0;
    return s->next_pid++;
}

void desk_sessions_init(struct desk_sessions *s)
{
    if (!s) return;
    /* Freestanding kernel: no implicit libc memset/memcpy dependency. */
    volatile unsigned char *bytes = (volatile unsigned char *)s;
    for (size_t i = 0; i < sizeof(*s); ++i) bytes[i] = 0;
    s->next_pid = 1;
    s->shell_pid = allocate_pid(s);
    s->editor.document = -1;
    s->editor.notice = "";
}

uint32_t desk_sessions_start(struct desk_sessions *s, enum desk_session_kind kind)
{
    if (!s || (unsigned)kind >= DESK_SESSION_COUNT) return 0;
    struct desk_session_record *r = &s->app[kind];
    if (r->running) return r->pid;
    const uint32_t pid = allocate_pid(s);
    if (!pid) return 0;
    r->pid = pid;
    r->running = true;
    return pid;
}

bool desk_sessions_stop(struct desk_sessions *s, enum desk_session_kind kind)
{
    if (!s || (unsigned)kind >= DESK_SESSION_COUNT || !s->app[kind].running)
        return false;
    s->app[kind].running = false;
    s->app[kind].pid = 0;
    return true;
}

uint32_t desk_sessions_pid(const struct desk_sessions *s, enum desk_session_kind kind)
{
    if (!s || (unsigned)kind >= DESK_SESSION_COUNT || !s->app[kind].running)
        return 0;
    return s->app[kind].pid;
}

uint32_t desk_sessions_restart_shell(struct desk_sessions *s)
{
    if (!s) return 0;
    const uint32_t new_pid = allocate_pid(s);
    if (!new_pid) return 0;
    s->shell_pid = new_pid;
    ++s->shell_restarts;
    return new_pid;
}

const char *desk_sessions_name(enum desk_session_kind kind)
{
    switch (kind) {
    case DESK_SESSION_EXPLORER: return "Проводник";
    case DESK_SESSION_EDITOR: return "Текстовый редактор";
    case DESK_SESSION_NOTES: return "Заметки";
    case DESK_SESSION_DOCUMENTS: return "Документы";
    case DESK_SESSION_PROCESSES: return "Процессы";
    default: return "Неизвестное приложение";
    }
}
