#ifndef STYLEOS_DESKTOP_INTERACTIVE_863_H
#define STYLEOS_DESKTOP_INTERACTIVE_863_H
#include <styleos/desktop_v0.h>
/* Q35/TCG only; no general device admission. */
void s863_shell_guard(void);
#ifdef STYLEOS_TEST_DESKTOP_PUMP_863
void s863_shell_pump(void);
#ifdef STYLEOS_TEST_DESKTOP_COLD_TASKS_863
bool s863_request_tasks(void);
#endif
#endif
void s863_shell_observe(const struct desk_event *,unsigned,unsigned,unsigned,bool,unsigned,size_t,uint64_t,bool,unsigned,size_t,unsigned,unsigned);
#endif
