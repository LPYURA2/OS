# StyleOS 766 — boundaries between native application processes, shell and user sessions

**Source base:** accepted 765 `b4d8a5dc9a66df744cae099ce03aaec980e7ef11`.
**Branch:** `feature/styleos-user-session-boundary-766`.

## Why this stage exists

The existing desktop has two distinct concepts that must NOT be confused:

1. `kernel/desktop_sessions.c` maintains five **legacy app UI session** records,
   a shell restart counter and the separate Editor/Notes draft buffers. Its
   `pid` fields are NOT hardware scheduler process IDs and are NOT inserted
   into the native task-manager registry.
2. `kernel/process_runtime.c`, `kernel/process_scheduler.c` and
   `kernel/desktop_process_runtime.c` own **real, scheduler-admitted PIDs**
   (CPL3 in the dedicated experimental image, no production .tea launch yet).
   Their windows are independently stored in `window_service`, not in the shell.

766 adds a third layer `user_session_runtime` which binds *only* live native
`PR_NATIVE_USER` entries to a trusted user session, retaining their real PID
and scheduler handle, independently of the window/compositor and shell lifecycle.

## Implemented in kernel-only model

- Up to four session slots, with never-reused monotonically increasing IDs,
  opaque account ID supplied exclusively by a future trusted auth broker,
  ACTIVE/LOCKED states, selection, locking and explicit close.
- Bind of a real PID only after successful scheduler admission and verification
  of its live state, native type, exact trusted handle and parent provenance.
  A parent PID must be in the same session; a kernel task is not an app.
- Read-only process enumeration for a bound live caller, restricted to their
  own session, exposing PID, parent PID, state and name only. No kernel handles,
  CR3 roots or fabricated CPU/RAM measurements are copied into the response.
- Compositor-facing `usr_visible_windows()` filters the *actual* `ws_window`
  registry using real process owner PIDs and active selected session. The
  ownerless trusted installation modal is not exposed through this application
  view. Locking yields no visible user windows. A shell restart never modifies
  the session or process registries.
- Fault/exit reconciliation calls `ws_reap_dead_owners` and then removes stale
  PID-to-session links; session close refuses live/uncleared links.
- Ordinary Desktop initializes this kernel service **empty** after the existing
  process dispatcher. It emits `STYLEOS USER SESSION SERVICE READY sessions=0`
  and proceeds with the existing desktop unchanged, with no fake accounts.

## Verification

```
python3 tools/test_user_session_runtime.py --output build/user-session-host
python3 tools/build.py --arch x86_64 --desktop-v0 --output build/desktop-766
```

Host test uses ASAN+UBSAN and a freestanding COFF build. It exercises two
separate sessions and real registry PIDs, parent restrictions, trusted-handle
mismatches, kernel-task denial, app-owned windows and independent installer
modal, shell/compositor restart, process cleanup and no session ID reuse.
The branch CI additionally runs the complete source/ISO/QEMU recovery suite
and a real UEFI serial marker assertion. CI must finish with success before
this stage can be accepted.

## Strict limitations (not a user-login release)

- This module is a kernel-side **policy and ownership model**, not a real
  authenticated login, encrypted credential store, per-account home directory,
  file permissions, `user` syscall, ACL, security boundary enforced in an
  independently launched native application, or a display manager.
- The current desktop still draws its historical shell-integrated UI apps;
  their UI session IDs are deliberately not admitted as real PIDs.
- `usr_select()` and `usr_create()` are **privileged trusted-caller APIs**.
  Never wire them to untrusted app input without a separate authentication
  and authorization broker. In particular, an opaque account ID is NOT an
  authentication token, and the model does not protect memory itself.
- Synchronization, scheduler locking, SMP, timeouts, per-user filesystem
  namespaces and CPU/RAM accounting are pending. No fake CPU/%RAM chart.
- The memory program from 762–765 is still needed for real executable `.tea`
  processes. 766 does not switch CR3 or claim new physical pages.
- The login/chooser screen MUST be implemented from the user's approved
  design mockup, not invented or copied from Windows. Existing desktop is
  retained until a tested session manager can replace its shell-only view.

## Next milestone — 767

Continue two strands with explicit dependencies:
1. Prove the desktop-safe x86-64 root and real per-process memory ownership
   (PE section W^X, GDT/IDT/TSS/stack, UART, framebuffer and MMIO) before
   enabling native `.tea` in CPL3.
2. Build a transactional native `.tea` launch/exit contract connecting
   prepared memory root -> real process registry PID -> session ownership ->
   window service. Reject failed admission without orphaned PIDs or page
   grants. Then provide authenticated, bounded syscall/IPC. Later implement
   actual user accounts, filesystem isolation and the approved login screen.
