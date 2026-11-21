# StyleOS Process Snapshot — kernel-side staging v1

Base: `e6e74c4d6f0e93fd00e8abdc8115f93aeb721510`. Task branch: `feature/styleos-process-snapshot-api-759`.

## Purpose

Prepare a **read-only, capability-gated process snapshot** for the future independent `Processes.tea`, keeping the existing desktop, F8 legacy panel, and approved UI untouched. `include/styleos/process_api.h` and `kernel/process_api.c` are compiled into the desktop EFI image but are NOT yet a user-mode syscall or IPC endpoint. No binary `Processes.tea` exists here. No new process is admitted in the boot path.

`pa_read()` accepts only kernel-side buffers and a caller PID/capability already authenticated by a **trusted kernel broker**, rejects an unauthenticated, dead or ungranted caller before returning counts, and refuses invalid version/sort/search/reserved flags and excess capacity. Results are fixed-width records with PID, PPID, group, state, memory bytes, raw CPU ticks, window count and label; **scheduler handles and page roots are deliberately not disclosed**. The legacy shell window list never creates a PID.

## Truthfulness and security limits

- `measurement_flags` is **zero** until real scheduler CPU accounting and resource measurement are connected. Raw counters in test fixtures do not prove real guest CPU usage or RAM consumption; no CPU percentage or graphs are exposed.
- This is **not** a stable syscall ABI yet. Future CPL3 wrapper must copy and bounds-check the entire fixed-size request from user address space and authenticate the actual caller/inspection permission using kernel state. Never accept `kernel_authenticated_pid` or `kernel_inspect_granted` from app parameters.
- The future broker must synchronize the process registry and window service while collecting a snapshot. This staging code has no IRQ, scheduling locks or concurrency guarantees.
- No end-task, process signal, shell-session bridge, .tea execution, independent user-mode address space or service control is added. Critical kernel tasks remain non-terminable from UI.
- F10 compositor restart must preserve live PID data; any legacy F8 test remains a **session** test and cannot be presented as a real process test.

## Checks

```sh
python3 tools/test_process_api.py --output build/process-api-host
python3 tools/test_process_view.py --output build/process-view-host
python3 tools/test_process_runtime.py --output build/process-host
python3 tools/build.py --arch x86_64 --desktop-v0 --output build/process-api-efi
```

The first test compiles with ASAN/UBSAN and cross-compiles the kernel API for x86-64 COFF. GitHub CI is configured to run the existing full 709-to-730 regression matrix on this task branch; a local host PASS is not a QEMU PASS.

## Next real milestone

1. Design and implement a production task scheduler and its lifecycle/accounting events, first with two distinct verified x86-64 CPL3 processes.
2. Add kernel-authenticated, memory-safe syscall/IPC wrapper and grants for the standalone `Processes.tea`. Do not expose raw kernel pointers to CPL3.
3. Measure CPU elapsed/idle windows and resident allocated pages; only then mark corresponding telemetry flags as valid and render performance graphs.
4. Build and boot the native `Processes.tea`, with independent PID and approved StyleOS UI. Test app launch/fault/exit, F10 persistence, permissions and protected termination separately.

This stage is a security/transport preparation step, **not** completion of the scheduler or Processes.tea.
