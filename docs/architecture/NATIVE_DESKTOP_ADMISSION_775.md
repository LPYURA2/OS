# StyleOS 775 — atomic kernel process/window IPC admission

Base: accepted 774, `8f5b5796da5948bcc80ed0d270ad4fd846a36c99`, CI `44951245909` PASS.
This stage closes a **kernel-side transaction gap** between the previously
verified `native_launch_stage` (true PID + page lease + selected session +
system window) and `native_window_channel` (capability-checked IPC port).

## New kernel-only coordinator

`native_desktop_launch` calls `nla_commit` to atomically admit a scheduler
PID, lease the prepared root, bind the selected session and open a system
window. It then calls `nwc_claim` **before the new context is runnable**.
Both must succeed for the coordinator to publish a launch result. No PID,
window ID, root, handle or lease is guessed or allocated by this layer.

If the IPC port claim fails, the process is still READY and has never been
executed: the broker marks it EXITED, calls `nla_finalize`, removes its window,
revokes and scrubs the detached root, reaps the PID and drops stale ports.
The consumed PID is never reused, and revoked frames cannot be retried.
If teardown itself fails, the broker reports `NDL_ROLLBACK_FAILED` rather than
claiming a clean rollback. It never returns a success result for an incomplete
launch. The coordinator is single-CPU only and assumes scheduler exclusion.

After a trusted architecture backend proves all CPUs/traps released the root,
`ndl_reap` removes a terminal process lease and reconciles its IPC port. It
cannot reclaim physical RAM or itself establish CPU quiescence. It rejects
attempts to reap still-live PIDs and stale handles.

## Tests

- Host fixture uses real `pr_table`, `ps_dispatcher`, `dfo_owner`,
  `usr_runtime`, `ws_service`, `nwc_service`, and the actual 774 presenter.
- Two independent real PIDs get separate prepared roots, selected sessions,
  windows and IPC ports. Kernel-owned tile reaches `ndb_paint`, and user
  session switching hides the foreign pixel.
- Compositor restart keeps a live port/tile, but a process FAULT or EXIT
  revokes only its own port and user root. Other processes continue.
- A synthetic *test fault* exhausts the eight IPC port slots; the still-READY
  process is rolled back with zero orphaned PIDs/windows or live leases.
- Wrong session, stale root, double admission/reap and live-process reap fail.
- ASAN+UBSAN, strict `-Werror` COFF compilation and standard desktop EFI link.

CI requires the entire existing host and real QEMU/OVMF suite to pass for
this exact commit. This is not an x86 hardware end-to-end ELF GUI run.

## What this does **not** accomplish

`ndl_admit` does NOT parse `.tea`, load ELF, reserve physical pages, install
GDT/TSS, start a user instruction, implement a production CPL3 syscall ABI,
authenticate users, route GUI focus/input or display a complete window.
The ordinary Desktop links the coordinator but never calls it at boot:
there are zero actual user PIDs, prepared page roots and IPC ports by default.
The previous legacy desktop windows continue to work as before. This code
must NOT be called with untrusted `nla_request` data or with a context that has
already run. No secure accounts, file ACLs or finished Explorer are claimed.

## Next step

776 should provide a trusted production memory/ELF package loader and bounded
launcher authority feeding this coordinator, then independently prove in QEMU
that a disk-loaded `.tea` executes *while the normal Desktop remains alive*
with real syscall, keyboard delivery, per-process composited pixels and proper
reaping. Do this in isolated profiles first. Do not weaken the user-root,
PS/DFO, rollback or session invariants. Once native apps are truly independent,
implement approved login chooser and secure users, then StyleFS homes/ACLs and
Explorer finalization according to approved mockups (`Быстрый доступ`).
