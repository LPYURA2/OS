# StyleOS 774 — kernel-owned native window tiles in the ordinary Desktop composer

Base: accepted 773, commit `0314f4d4e5de803ec9ff3cd344ee7761b95d44e0`, CI 44949466729 PASS.
This change is **not** a general RAM manager, `.tea` installer, independent window
creation path, production CPL3 syscall or graphical app execution. It is one
presentation-boundary integration slice from the already compiled kernel IPC
service to the standard desktop's existing dirty-strip renderer.

## Changes

- `native_desktop_bridge` inspects the process-owned system-window registry,
  taking only **kernel-owned copies** of tiles from `nwc_visible_tile()`. That
  API authenticates the live PID, scheduler handle, user page root, window
  owner, selected and ACTIVE session, visibility and minimization. No raw
  application pointer can enter the presenter.
- A 16x16 maximum XRGB8888 tile is painted by a bounded color-only callback.
  Pixel coordinates are clipped to the **intersection** of the window bounds,
  tile bounds, display width and desktop work-area height. Nothing draws onto
  the taskbar or outside framebuffer bounds.
- `ndb_track` compares exact window ID, tile generation and clipped location
  against the last frame. It damages both the old and new regions after
  a new tile, size/move, window closure, minimization, session switch/lock
  or owner fault. Standard `desk_present()` restores the scene underneath.
- In the normal boot the PID registry, user sessions and IPC ports start empty.
  No synthetic process or tile is created by the bridge; previous Desktop
  layout, app windows, input handling, saved files and image styles are unchanged.
- The native tile is composited behind the desktop taskbar and cursor. There
  is no native window frame, focus management or general-sized surface yet.
  A 16x16 tile is not a full independent graphical window.

## Security/limitations

Single CPU, no new locks; the bridge assumes it is invoked by the trusted
compositor while the kernel window registry is not concurrently mutated.
No user input is sent by the bridge, and user-supplied coordinates or tile
sizes are never trusted. It does not authorize new allocations, account
creation or physical page grants. Real disk-loaded `.tea` processes still
cannot run through ordinary desktop launch and syscalls. This is a production
presentation path **ready to use when a genuine native window port exists**,
not proof that such a port is created at ordinary boot.

## Verification

```
python3 tools/test_native_desktop_bridge.py --output build/native-desktop-774-host
python3 tools/build.py --arch x86_64 --desktop-v0 --output build/native-desktop-774-efi
python3 tools/test.py --host --output build/native-desktop-774-regressions
```

The sanitized host fixture links the genuine `desktop_draw.c` strip renderer.
It verifies actual framebuffer pixels for partially offscreen tiles,
owner rejection, re-generation, compositor restart, minimized window,
active/locked/foreign user sessions, process fault, window close and port
reconciliation. The EFI build establishes production linker compatibility.
A successful full GitHub CI/QEMU run for the exact commit is required for
acceptance. A QEMU smoke test cannot prove that independent `.tea` windows
are already created on-screen in the ordinary Desktop.

## Next stage 775

Add safe kernel-owned process launch state and an explicitly scoped bridge
that connects real disk-loaded ELF64 `.tea` processes with native window
ports. Do not activate experimental address spaces in production without
hardware-lifetime proofs; do not invent user PID or CPU/RAM metrics. Only
when real independently running graphical apps and secured sessions exist
implement the approved login chooser, account storage and file permissions,
then finalize StyleFS and Explorer by their approved mockups.
