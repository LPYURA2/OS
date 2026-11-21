#include <styleos/desktop_editor_runtime.h>

static struct desk_editor_runtime editor_state;
static bool initialized;

struct desk_editor_runtime *desk_editor_runtime_open(void)
{
    if (!initialized) {
        /* BSS is zeroed at boot; this initialization must happen only once
         * per boot, never on an F10 shell rebuild. */
        editor_state.document = -1;
        editor_state.session_id = 1;
        initialized = true;
    }
    return &editor_state;
}
