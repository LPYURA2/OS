# StyleOS — new chat continuation after 772

Repo: `LPYURA2/OS`.
Prior accepted base: 771, commit `96834d3b48d81f577b3dce9af4d8d2286962b74f`, CI run `44945796971` PASS.
Stage branch: `feature/styleos-cpl3-user-copy-772`.
**Read actual final commit SHA / GitHub CI result before claiming 772 accepted.**

Objectives: native `.tea` independent processes, genuine process manager,
user sessions and separate process-owned windows. 772 is an x86-64 dedicated
CPL3 INT80 user→kernel copy + foreign-page denial **hardware test**, not a
Desktop user syscall. Old 768–770 ELF64 from `.tea` hardware tests and 771
boot-only empty desktop native window IPC are preserved independently.

Key files: `kernel/user_copy_772.c`, `include/styleos/user_copy_772.h`,
`arch/x86_64/user_processes.c`, `arch/x86_64/user_processes_entry.S`,
`kernel/user_processes_demo.c`, `tools/test_user_copy_772.py`,
`tools/test_user_syscall_772_guest.py`, `docs/architecture/CPL3_USER_COPY_BOUNDARY_772.md`.

**Next step 773:** authenticated versioned userspace IPC request, copy_to_user,
validation of root/PID/page rights and lifetime, then actual
`native_window_channel` event/tile integration in a test guest; later migrate
to production desktop once safe active memory management exists. No real
separate `.tea` window is drawn yet. No CPU%/RAM figures without measurements.

After process/application split is real, implement authenticated accounts and
login selection according to approved design mockups; then StyleFS user homes,
ACL/integrity and Explorer finalization. Preserve the rich current Desktop and
`Быстрый доступ` (never `Рекомендованное`).

Rules: every stage a separate GitHub task branch; no changes to `main` or old
accepted releases; show real QEMU logs and results; provide exact SHA complete
source ZIP, CI ISO, evidence, and next chat continuation. Do not claim that
isolated test guest coverage is ordinary desktop functionality.
