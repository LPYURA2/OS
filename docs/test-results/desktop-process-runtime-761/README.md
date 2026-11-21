# StyleOS 761 — desktop-owned process runtime bootstrap

Date: 21 November 2026. Base: accepted `a91c94242122544ec773ed6f36bfaa6ebb03ffc8`
(branch `feature/styleos-process-runtime-760`). Task branch:
`feature/styleos-desktop-process-bootstrap-761`.

## Scope

This is the **first production desktop initialization** of the bounded
cooperative dispatcher from 760, not yet production user-mode scheduling.
Desktop boot reads the current *real x86-64 CR3* under paging/PAE, masks its
non-root low bits, and initializes a kernel-owned `dpr_service` connected to
`pr_global()` and `ws_global()` after checking both are empty. A failed
bootstrap halts rather than silently populating fabricated entries.

- `kernel/desktop_process_runtime.c`: an independent kernel lifecycle bridge;
  trusted registration, scheduling selection and validated trap acceptance;
  on exit/fault cleans windows belonging to dead owners; releases PID only
  after the architecture backend has already disposed of its context.
- `arch/x86_64/desktop_process_arch.c`: read-only privileged capture of the
  current page-root address; no root switch or new mappings.
- `kernel/main.c`: connects the service **before the existing desktop shell**,
  asserts zero registered processes and emits
  `STYLEOS DESKTOP SCHEDULER READY registered=0`.
- `tests/test_desktop_process_runtime.c`: sanitizer host fixture tests
  boot/refusal, actual registry PID allocation, two context transitions,
  invalid hardware evidence refusal, F10/window persistence, fault/exit
  window cleanup, snapshot permissions, unavailable resource metrics.
- `tools/test_process_guest_boot.py`: the real guest must now show the new
  startup marker while preserving the prior `PROCESS SERVICE` / `WINDOW`
  markers and normal desktop screen.

## Hard safety boundary

**No user-mode processes are started in ordinary desktop StyleOS 761.**
Only the dedicated x86-64/ARM64 test profile actually executes two
independent CPL3/EL0 contexts (760). This production bootstrap uses the
existing firmware-inherited root *for identification only*: its page-table
ownership, safe kernel closure and lifetime have **not** been proven for
switching away from it and back. Do not enable `dpr_admit` in ordinary
Desktop or use this root for real user entry before an independent arch
ownership and mapping audit is completed.

The desktop `.tea` icons and F8 session panel are **not** true OS processes;
they are not admitted. No real `Processes.tea` is running, no CPL3 syscall or
user pointer copy, no timer/IRQ preemption, no asynchronous concurrency locks,
no CPU utilization sampler, no resident page accounting, no automatic task
termination. The process API's `measurement_flags` remains zero.

## Acceptance tests

```sh
python3 tools/test_desktop_process_runtime.py --output build/desktop-runtime-host
python3 tools/test_process_scheduler.py --output build/scheduler-host
python3 tools/test_process_api.py --output build/process-api-host
python3 tools/build.py --arch x86_64 --desktop-v0 --output build/desktop-761
# Hardware QEMU/OVMF in GitHub CI, with every existing 709→730 regression:
python3 tools/test_process_guest_boot.py --iso build/StyleOS-761.iso --output build/guest-runtime
```

QEMU guest PASS and full regression result must be read from the branch's
GitHub Actions run; a local COFF/EFI build is **not** boot proof. Old source
checkpoints, desktop Shell/Draw and format `.tea` remain unchanged.

## Next: actual native applications

1. Prove explicitly owned x86-64 kernel-safe tables and kernel entry/TSS/IDT
   in the desktop profile, with no loss of existing EFI framebuffer/IO.
2. Introduce an architecture-owned creation/release transaction for an
   independently mapped `.tea` executable with its own address space and
   user stack. Only after successful proof call `dpr_admit`.
3. Add a copying, bounded, kernel-authenticated syscall broker for process
   snapshots; convert `Processes.tea` from package/catalog to a running
   application, with permission and data-lifetime tests.
4. Add resource accounting based on observed CPU clock intervals and resident
   mappings; *then* show CPU %, memory and performance graphs.

This incremental bridge is not the accepted final process manager.
