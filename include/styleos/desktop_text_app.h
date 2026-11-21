#ifndef STYLEOS_DESKTOP_TEXT_APP_H
#define STYLEOS_DESKTOP_TEXT_APP_H
#include <styleos/desktop_sessions.h>
/* Independent Editor/Notes document-state mutation. No window/compositor API. */
void desk_text_app_replace(struct desk_editor_draft *draft,
                           size_t lo, size_t hi, uint32_t cp, size_t text_limit);
#endif
