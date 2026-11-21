#include <styleos/desktop_sessions.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void basic(void)
{
    struct desk_sessions *s = desk_sessions_global();
    desk_sessions_init(s);
    assert(s->shell_pid == 1 && s->next_pid == 2);
    assert(s->editor.document == -1 && s->editor.notice != NULL);
    const uint32_t exp = desk_sessions_start(s, DESK_SESSION_EXPLORER);
    const uint32_t edit = desk_sessions_start(s, DESK_SESSION_EDITOR);
    assert(exp == 2 && edit == 3);
    assert(desk_sessions_start(s, DESK_SESSION_EDITOR) == edit);
    assert(desk_sessions_pid(s, DESK_SESSION_EDITOR) == edit);
    assert(desk_sessions_pid(s, DESK_SESSION_DOCUMENTS) == 0);
    assert(strcmp(desk_sessions_name(DESK_SESSION_DOCUMENTS), "Документы") == 0);
    assert(desk_sessions_start(s, DESK_SESSION_COUNT) == 0);
    assert(!desk_sessions_stop(s, DESK_SESSION_COUNT));
    assert(desk_sessions_pid(NULL, DESK_SESSION_EDITOR) == 0);
    assert(desk_sessions_start(NULL, DESK_SESSION_EDITOR) == 0);
    assert(desk_sessions_restart_shell(NULL) == 0);
    puts("PASS: unique stable PIDs, idempotent start, refusal of invalid apps");
}

static void shell_restart_preserves_draft(void)
{
    struct desk_sessions *s = desk_sessions_global();
    desk_sessions_init(s);
    const uint32_t explorer = desk_sessions_start(s, DESK_SESSION_EXPLORER);
    const uint32_t editor = desk_sessions_start(s, DESK_SESSION_EDITOR);
    const uint32_t processes = desk_sessions_start(s, DESK_SESSION_PROCESSES);
    const char *draft = "unsaved \320\277\321\200\320\270\320\262\320\265\321\202";
    const size_t draft_len = strlen(draft);
    memcpy(s->editor.content, draft, draft_len);
    s->editor.length = draft_len;
    s->editor.caret = 12;
    s->editor.document = 17;
    s->editor.scroll = 3;
    s->editor.doc_folder = 2;
    s->editor.dirty = true;
    s->editor.notice = "Не сохранено";
    const uint32_t previous_shell = s->shell_pid;
    const uint32_t fresh_shell = desk_sessions_restart_shell(s);
    assert(fresh_shell != 0 && fresh_shell != previous_shell);
    assert(desk_sessions_pid(s, DESK_SESSION_EDITOR) == editor);
    assert(desk_sessions_pid(s, DESK_SESSION_EXPLORER) == explorer);
    assert(desk_sessions_pid(s, DESK_SESSION_PROCESSES) == processes);
    assert(s->editor.length == draft_len && s->editor.caret == 12);
    assert(s->editor.document == 17 && s->editor.scroll == 3);
    assert(s->editor.doc_folder == 2 && s->editor.dirty);
    assert(memcmp(s->editor.content, draft, draft_len) == 0);
    assert(strcmp(s->editor.notice, "Не сохранено") == 0);
    assert(s->shell_restarts == 1);
    puts("PASS: shell PID refresh preserves app PIDs and unsaved UTF-8 draft");
}

static void stop_and_reopen(void)
{
    struct desk_sessions *s = desk_sessions_global();
    desk_sessions_init(s);
    const uint32_t first = desk_sessions_start(s, DESK_SESSION_EDITOR);
    assert(desk_sessions_stop(s, DESK_SESSION_EDITOR));
    assert(!desk_sessions_stop(s, DESK_SESSION_EDITOR));
    assert(desk_sessions_pid(s, DESK_SESSION_EDITOR) == 0);
    const uint32_t second = desk_sessions_start(s, DESK_SESSION_EDITOR);
    assert(second > first);
    assert(desk_sessions_start(s, DESK_SESSION_NOTES) > second);
    assert(desk_sessions_start(s, DESK_SESSION_DOCUMENTS) > second);
    puts("PASS: close/reopen allocates a fresh PID, names remain distinct");
}

static void pid_exhaustion(void)
{
    struct desk_sessions *s = desk_sessions_global();
    desk_sessions_init(s);
    s->next_pid = UINT32_MAX;
    assert(desk_sessions_start(s, DESK_SESSION_EDITOR) == 0);
    assert(desk_sessions_restart_shell(s) == 0);
    assert(s->shell_pid == 1);
    assert(s->app[DESK_SESSION_EDITOR].running == false);
    puts("PASS: PID exhaustion refuses new processes without corrupting state");
}

int main(void)
{
    basic(); shell_restart_preserves_draft(); stop_and_reopen(); pid_exhaustion();
    puts("DESKTOP SESSION TESTS: PASS");
    return 0;
}
