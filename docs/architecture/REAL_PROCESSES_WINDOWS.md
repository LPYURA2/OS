# StyleOS: processes before native applications; independent window manager

Owner decision: 21 November 2026. The first full release must ship nine preinstalled standalone native .tea apps. 'Процессы' must be an independent native .tea and display actual system tasks and running applications, never synthetic desktop sessions.

## Added in this checkpoint (source + host tests)
- kernel/process_runtime.c: kernel-owned scheduler-event process registry. Monotonic process IDs, parent IDs, user/kernel task classification, actual handle and page-table reference, ready/running/waiting/exited/faulted states, CPU-tick/memory accounting, fault/exit/reap, snapshot of live entries. A record requires a trusted scheduler handle and, for user tasks, a nonzero aligned page-table root. Legacy F5 session IDs are expressly not inserted into this registry.
- kernel/window_service.c: window state resides outside desktop_shell.c; a native app window is owned by a live admitted user process. Non-owners cannot mutate or close another process's window. Shell/compositor generation restart does not destroy windows or their state; a faulted owner does. Window geometry and presentation are not hard-coded in the service.
- System installer modal data corresponds to approved StyleOS-System-Scenarios.pdf, pages 8-9: 'Установка приложения', and the incompatible case with install action disabled and TEA_INCOMPATIBLE. No package is installed by a modal: the installer must provide the separately verified compatibility verdict.
- Approved Settings mock also exists as StyleOS-Settings-Prototype.html. Explorer / Documents references remain their approved 20 / 14 screen albums.

## Deliberate limits
The production desktop build compiles these services but does not yet supply real scheduler admission events, CPL3 ELF launch, IPC, app-owned backing surfaces or guest window rendering. Therefore **the process viewer is not yet real or independent** and these host tests do not imply the nine .tea apps are running. The current F8 panel still uses legacy simulated sessions; it must be replaced only when the scheduler and process snapshot are connected. Until then do not present its PIDs as real processes.

Next acceptance: integrate scheduler task creation/fault/exit events with pr_global(), implement read-only process-snapshot syscall with permissions, create independent Processes.tea using the API, then prove in QEMU two real user processes (distinct address spaces/PIDs) and their exit/fault lifetime. Next connect window_service to real compositor IPC, implement installer modal and Deep Drop panels strictly per mocks. No changes to existing desktop/palette, desktop_shell.c or desktop_draw.c in this checkpoint.
