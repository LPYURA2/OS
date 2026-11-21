# StyleOS — continuation after stage 774

Repository: `LPYURA2/OS`.
Accepted base 773: `0314f4d4e5de803ec9ff3cd344ee7761b95d44e0`, GitHub Actions `44949466729` PASS.
Stage 774 branch: `feature/styleos-native-desktop-presentation-774`.

**Before calling 774 accepted, verify the exact head commit, GitHub Actions
result and source artifact. Never infer success from this file.**

Changes: `kernel/native_desktop_bridge.c`, `include/styleos/native_desktop_bridge.h`,
`kernel/desktop_shell.c`, `tools/build.py`, `tests/test_native_desktop_bridge.c`,
`tools/test_native_desktop_bridge.py`, and workflow/architecture docs.

774 adds a real pixel-copy presentation boundary: the ordinary Desktop's
strip framebuffer renderer can now consume the already-authenticated 16x16
kernel tile for a process-owned window. Dirty rectangles clear old pixels on
move/update/minimize/session lock/switch/FAULT/close. At boot there are
zero real PIDs, sessions and native IPC ports, so no synthetic windows appear.
Host ASAN/UBSAN tests exercise genuine pixel writes; full QEMU CI is mandatory.

**Unfinished:** ordinary Desktop does not launch an ELF64 `.tea` from disk in
CPL3, has no production syscall ABI, no general safe PMM, no app focus/input
routing, no real native window frame, account authentication or isolated files.
Do not misreport the model's 16x16 tile as a complete native GUI application.

**Next 775:** continue real user-process integration and bounded ownership/lifetime
across disk-loader, scheduler PID, page root, user session and native window port.
Preserve current rich Desktop and existing apps; use a new GitHub branch.
No main or previous refs changed, no fake PID/CPU/RAM. After independently
running native apps, create approved login chooser with secure accounts,
then StyleFS homes/permissions/recovery and finalized Explorer UI ("Быстрый доступ").

Each stage must deliver full exact-commit source ZIP, bootable ISO produced by
successful CI, QEMU proof and a new-chat continuation document. If CI is
red or still running, label the stage not accepted.
