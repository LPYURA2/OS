#include <styleos/desktop_editor_runtime.h>
#include <assert.h>
#include <string.h>

int main(void)
{
    struct desk_editor_runtime *original = desk_editor_runtime_open();
    assert(original != NULL);
    assert(original->document == -1);
    assert(original->session_id != 0);
    const char draft[] = "Never lose a draft";
    memcpy(original->content, draft, sizeof(draft) - 1);
    original->length = sizeof(draft) - 1;
    original->caret = original->length;
    original->dirty = true;
    original->document = 3;
    original->folder = 2;
    original->scroll = 7;

    /* Reopening the runtime corresponds to rebuilding the shell. */
    struct desk_editor_runtime *reopened = desk_editor_runtime_open();
    assert(reopened == original);
    assert(reopened->session_id == original->session_id);
    assert(reopened->document == 3);
    assert(reopened->folder == 2);
    assert(reopened->scroll == 7);
    assert(reopened->dirty);
    assert(reopened->caret == sizeof(draft) - 1);
    assert(reopened->length == sizeof(draft) - 1);
    assert(!memcmp(reopened->content, draft, sizeof(draft) - 1));
    return 0;
}
