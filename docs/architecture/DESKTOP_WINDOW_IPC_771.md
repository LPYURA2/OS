# StyleOS 771 — kernel-owned native window channel, desktop bootstrap

Source: accepted 770, commit `3d8115d6a72b60d7fdf5ed2a2ce11cf5e847d6da`.
Task branch: `feature/styleos-desktop-window-ipc-771`.

## Objective and actual scope

Move one bounded part of native application/desktop integration into the real
x86-64 ordinary desktop boot: a **kernel-owned window IPC staging service**.
It does not create an account, a new app, a fake process or a fake surface.
It initializes only after the actual process registry, window service, and
user-session runtime have started; at boot it reports `ports=0`.

All application-specific behavior is exercised by a host test fixture with
real process-registry PIDs and roots **admitted inside that fixture**, not by
ordinary desktop user-mode apps. The existing F5/F8 window-session IDs remain
separate from kernel PIDs. The desktop shell/approved drawings are unchanged.

## API: `native_window_channel.h`

- `nwc_claim`: trusted kernel-only admission after validated PID, hardware
  handle, page root, session membership and process-owned window.
- `nwc_send_key` / `nwc_send_text`: bounded kernel-owned queue (four events per
  port, maximum 32 copied text bytes). Enqueues are allowed only when the
  window belongs to the **selected ACTIVE** user session.
- `nwc_receive`: requires matching PID, handle and root and an active selected
  session. A failed call does not overwrite the output or consume an event.
- `nwc_publish_tile`: copies one XRGB8888 tile of up to 16x16 pixels from
  a **kernel-owned copied buffer** to an independently owned window.
  Unused bytes are cleared on every update. The compositor-only snapshot is
  hidden for locked, foreign, exited and closed windows.
- `nwc_reconcile`: revokes IPC ports and clears pending events/pixel tiles
  after windows close or processes fault/exit, without destroying the
  process/physical allocations on the channel's own authority.
- Compositor restart only increments the compositor generation; kernel-held
  window state/tile remains. Session switches isolate visibility/input.

## Critical security caveats

`nwc_*` functions are **not** callable from user mode. Their pointer arguments
refer to staging buffers in trusted kernel memory. Actual CPL3 trap handler,
read-only copy_from_user/copy_to_user, pointer range checking, reentrancy,
locks, physical memory ownership, common syscall ABI, input driver and
raster/display compositor are *not implemented* in 771. Treating a user
supplied PID/handle/root as authentication would be a vulnerability. The
hardware scheduler/trap layer must authenticate current context itself.

The existing kernel/user session runtime is a **model** and its account IDs
are unauthenticated test tokens. No screen login, ACL or secure user home
folders yet. The GUI panel for Processes still does not have real production
user PIDs, CPU utilization or measured resident memory.

## Tests

```sh
python3 tools/test_native_window_channel.py --output build/771-window-host
python3 tools/build.py --arch x86_64 --desktop-v0 --output build/771-desktop
python3 tools/test_process_guest_boot.py --iso build/StyleOS-771.iso --output build/771-qemu
```

The first test runs ASAN/UBSAN, negative authorization and overflow checks,
checks actual registry PIDs, compositor persistence, state isolation and
cleanup, and compiles the new service to freestanding x86-64 UEFI COFF.
The last test is meaningful only with the actual QEMU/OVMF guest image built
in GitHub CI. A host-only test must not be described as hardware proof.

## Follow-up

772: author a real kernel trap/copy contract on **one dedicated x86-64 profile**:
copy bounds, root-bound caller authentication, early fault containment and
small host/guest positive+negative probes. After independent proof, connect
this channel to a true process-owned display surface and keyboard input in
ordinary Desktop, leaving verified F5/F8 UI unchanged. Implement authenticated
accounts/login mockup, StyleFS authorization and Explorer finalization only
after process/session separation is enforced in the ordinary boot path.
