#include <styleos/system_apps.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char *const expected_id[9] = {
    "org.styleos.explorer",
    "org.styleos.settings",
    "org.styleos.notes",
    "org.styleos.editor",
    "org.styleos.documents",
    "org.styleos.spreadsheets",
    "org.styleos.presentations",
    "org.styleos.browser",
    "org.styleos.processes"
};
static const char *const expected_name[9] = {
    "Проводник",
    "Параметры",
    "Заметки",
    "Текстовый редактор",
    "Документы",
    "Таблицы",
    "Презентации",
    "Браузер",
    "Процессы"
};
int main(void)
{
    assert(styleos_system_app_count() == 9);
    for (unsigned i = 0; i < 9; ++i) {
        const struct styleos_system_app *a = styleos_system_app_at(i);
        assert(a && a == styleos_system_app_find(expected_id[i]));
        assert(strcmp(a->id, expected_id[i]) == 0);
        assert(strcmp(a->display_name, expected_name[i]) == 0);
        assert(strncmp(a->package_path, "/System/Applications/", 21) == 0);
        size_t n = strlen(a->package_path);
        assert(n > 4 && strcmp(a->package_path + n - 4, ".tea") == 0);
        for (unsigned j = 0; j < i; ++j)
            assert(strcmp(a->id, expected_id[j]) != 0);
    }
    assert(!styleos_system_app_at(9));
    assert(!styleos_system_app_find(0));
    assert(!styleos_system_app_find("org.styleos.missing"));
    assert(!styleos_system_app_find("org.styleos.editor.sapp"));
    puts("SYSTEM APP CATALOG HOST PASS: nine unique native .tea apps");
    return 0;
}
