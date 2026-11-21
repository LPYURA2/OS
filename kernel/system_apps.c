#include <styleos/system_apps.h>
/* This catalog lists required packages; it does NOT imply they are installed
 * or executed. The compositor owns no application implementation here. */
static const struct styleos_system_app required_apps[] = {
    {"org.styleos.explorer", "Проводник", "/System/Applications/org.styleos.explorer.tea"},
    {"org.styleos.settings", "Параметры", "/System/Applications/org.styleos.settings.tea"},
    {"org.styleos.notes", "Заметки", "/System/Applications/org.styleos.notes.tea"},
    {"org.styleos.editor", "Текстовый редактор", "/System/Applications/org.styleos.editor.tea"},
    {"org.styleos.documents", "Документы", "/System/Applications/org.styleos.documents.tea"},
    {"org.styleos.spreadsheets", "Таблицы", "/System/Applications/org.styleos.spreadsheets.tea"},
    {"org.styleos.presentations", "Презентации", "/System/Applications/org.styleos.presentations.tea"},
    {"org.styleos.browser", "Браузер", "/System/Applications/org.styleos.browser.tea"},
    {"org.styleos.processes", "Процессы", "/System/Applications/org.styleos.processes.tea"}
};
unsigned styleos_system_app_count(void)
{ return (unsigned)(sizeof(required_apps) / sizeof(required_apps[0])); }
const struct styleos_system_app *styleos_system_app_at(unsigned index)
{ return index < styleos_system_app_count() ? &required_apps[index] : 0; }
static int eq(const char *a, const char *b)
{
    if (!a || !b) return 0;
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == *b;
}
const struct styleos_system_app *styleos_system_app_find(const char *id)
{
    if (!id) return 0;
    for (unsigned i = 0; i < styleos_system_app_count(); ++i)
        if (eq(id, required_apps[i].id)) return &required_apps[i];
    return 0;
}
