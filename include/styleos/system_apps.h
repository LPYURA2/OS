#ifndef STYLEOS_SYSTEM_APPS_H
#define STYLEOS_SYSTEM_APPS_H
/* Independent native-app identity, not a user PID or a shell window. */
struct styleos_system_app {
    const char *id;
    const char *display_name;
    const char *package_path;
};
unsigned styleos_system_app_count(void);
const struct styleos_system_app *styleos_system_app_at(unsigned index);
const struct styleos_system_app *styleos_system_app_find(const char *id);
#endif
