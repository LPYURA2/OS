# StyleOS — continuation after stage 775

Repository: `LPYURA2/OS`.
Last accepted base 774: `8f5b5796da5948bcc80ed0d270ad4fd846a36c99`, CI 44951245909 PASS.
Stage branch: `feature/styleos-native-desktop-launch-775`.

**Do not mark 775 accepted before checking its final exact SHA, green GitHub
Actions run and the source and ISO artifacts.** Read `AGENTS.md`,
`docs/PROJECT.md`, `docs/architecture/NATIVE_DESKTOP_ADMISSION_775.md`,
`docs/architecture/NATIVE_LAUNCH_TRANSACTION_767.md`,
`docs/architecture/DESKTOP_WINDOW_IPC_771.md` and
`docs/architecture/NATIVE_DESKTOP_PRESENTATION_774.md`.

Stage 775 adds `include/styleos/native_desktop_launch.h` and
`kernel/native_desktop_launch.c`: a kernel-only, pre-dispatch coordinator
which couples PID/lease/session/window admission to an IPC port claim.
Its failure path marks a still-READY process terminal and revokes the detached
root; it cannot re-use failed memory. Terminal faults/exit revoke the IPC port
and do not disturb other PIDs. The test links real process/scheduler/frame/
session/window/IPC/presenter modules and verifies a pixel from an actual
port is processed by the 774 compositor with session isolation.

**Important boundaries:** The ordinary Desktop links this module but has no
production ELF loader, account authentication, safe PMM, CPL3 syscall entry
or actual independent `.tea` app running alongside the shell yet. Do not
claim the host pixel test proves a live on-screen independent GUI app.

**776 goal:** safely integrate verified disk package admission, actual
architecture user roots, scheduler and syscall callback to the new transaction,
then prove an independent graphical `.tea` runs while Desktop is alive.
Prove cleanup on FAULT/EXIT and two sessions, with QEMU/OVMF. Small incremental
safe steps; don't create fake PIDs/CPU/RAM numbers. Do not modify `main` or
approved desktop mockups. After independent apps: approved login chooser and
real account/session authentication; then StyleFS home dirs, permissions,
recovery and final Explorer UI (`Быстрый доступ`, not `Рекомендованное`).

For every stage: create fresh branch, pass full host + QEMU CI, deliver exact
commit source ZIP, QEMU bootable ISO, evidence and a new-chat continuation.
