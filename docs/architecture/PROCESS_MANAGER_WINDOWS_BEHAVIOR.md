# Processes.tea — Windows Task Manager ergonomics without copying its UI

Owner specification (21 November 2026): **Processes** is an independent native `.tea` system application, analogous in purpose to Windows Task Manager, showing real OS kernel tasks and running programs. Familiar Windows behaviours are allowed; approved StyleOS visual design and current desktop/palette remain unchanged.

## Implemented kernel-side model (not yet a guest Processes.tea)
- Read-only `process_inspector` snapshot, separate from desktop shell and app session registry.
- Group 1 **Приложения**: active real user processes that own windows, including minimized.
- Group 2 **Фоновые процессы**: active user processes without app-owned windows.
- Group 3 **Системные процессы**: admitted kernel tasks.
- Each row carries **real PID**, parent PID for hierarchical display, running/waiting/ready state, raw scheduler CPU ticks, observed memory bytes, number of windows, and canonical name.
- A compositor F10 restart preserves process registry and windows; process fault removes live row and its windows. An OS-owned installer dialog does **not** invent a new process row.
- Kernel processes must not be ended from UI; `pi_can_request_end` only returns a policy verdict for ordinary apps, not an end-process operation. Actual termination requires a new authenticated scheduler IPC capability, no direct UI mutation.
- Until the scheduler has supplied trustworthy wall-clock interval and CPU time accounting, CPU % is explicitly marked **unavailable**. Do not paint fictional values or infer CPU usage from window focus. Memory data originates from kernel resource accounting, not guessed by the UI.

## UI acceptance after real .tea execution
Familiar Task Manager functions: grouping, expand process trees, resource indicators, sort/filter/search, process details, safe end-task request, independent process-viewer surface, diagnostics and service controls. App process must survive compositor restart, with its own user-mode address space. Processes.tea needs read-only process telemetry grants; terminating an eligible user task needs explicit capability and confirmation.

All visual elements follow approved StyleOS mocks. No redrawing the current desktop or editing desktop_shell.c/desktop_draw.c at this stage.

Current truth: kernel registry, window service, and read-only inspector have host/EFI compile tests. They are not yet wired into a production scheduler/IPC or a guest Processes.tea. Historical F8 UI still shows legacy session IDs; do not label them as real processes.
