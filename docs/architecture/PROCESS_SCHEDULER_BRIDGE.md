# StyleOS — system PID life cycle: two-context hardware bridge

Base: `3ffe22b7fcf843f10d37f4a9b5e852500bb14f01` (verified 759).
Branch: `feature/styleos-process-runtime-760`.

## What was actually added

A two-slot kernel-only **cooperative dispatcher** (`kernel/process_scheduler.c`) registers the two already existing, verified test contexts (CPL3 x86-64 / EL0 AArch64) into the same `pr_table` used by the task-manager process inspector. Admission uses an actual prepared hardware context address and its separate address-space root. The guest profile drives a round-robin selection, and transitions PID records between READY, RUNNING, READY (yield), FAULTED or EXITED on **validated physical exception/trap events**. Only after the underlying `up_release` does it reap the registry records. It checks monotonic PID and consistency after every turn. It **does not** create processes from desktop window sessions.

The existing `up_arch_run`, CPL3/EL0 assembly, verified exception frames, CR3/TTBR handling and strict trace parser were reused. The guest-profile code has no new UART marker: a preexisting `STYLEOS USER PASS` is reached **only if the additional PID state assertions also pass**. The original desktop, its window/session-based F8 panel, process viewer and read-only snapshot API are not altered.

## Scope limitations

- This is real privileged-to-user execution evidence **only in the dedicated test profile**; it is not the desktop process scheduler, a general launcher, or standalone `Processes.tea`.
- Scheduling is **cooperative**, test-bounded to two contexts, with a round-robin handoff at existing trap boundaries. There are no timer interrupts, time slices, preemption, IPC, process kill or generalized admission of `.tea` apps.
- PID comes from the kernel registry, never from a window. `resident_bytes = 0` deliberately: physical resident-memory accounting has not been connected. `cpu_ticks` and CPU % are NOT measured here. `PA.measurement_flags` remains zero.
- `ps_accept` expects a **trusted architecture caller** that verified trap origin, user root and kernel-safe root. It is NOT callable by arbitrary applications. The future dispatcher must enforce synchronization/IRQ locks and a safe context-switch/teardown path.
- System critical tasks remain unkillable from UI, and Windows-like behavior is a **functional reference** only; StyleOS desktop design stays independent.
- A host-mode state-machine PASS is not enough. Real hardware coverage is provided only if the separate QEMU/OVMF step succeeds; otherwise report guest as not confirmed.

## Tests

```sh
python3 tools/test_process_scheduler.py --output build/ps-host
python3 tools/build.py --arch x86_64 --test-user-processes --output build/ps-x86
python3 tools/build.py --arch aarch64 --test-user-processes --output build/ps-arm
python3 tools/test_process_scheduler_guest.py --arch all --output build/ps-guest
```

Host tests: ASAN+UBSAN, x86-64 COFF and AArch64 freestanding cross-compile; verify bad roots, duplicate handles, state changes on events, no mutation on rejected evidence, monotonic PIDs, fault/exit/reaping. Guest: the original strict `user_processes_report()` in `tools/test.py` verifies hardware context/roots/page isolation and trap details on x86-64 and AArch64 (EL1 and EL2 paths), plus guest-internal registry checks.

## Next: native desktop runtime

Do **not** populate the real desktop registry with fake PIDs from F5/F8. The next step is to extract the bounded isolated CPL3 path from the test-only `up_demo_run` into a real desktop-owned scheduler lifecycle with safe kernel/guest memory ownership. Add a real kernel-side source of interval CPU time and actual resident allocated pages before exposing `%CPU`, RAM utilization or performance charts. Then implement kernel-authenticated syscall/IPC for `Processes.tea` and full lifecycle for native `.tea` programs.
