# StyleOS — continuation after stage 776

Repository: `LPYURA2/OS`.
Accepted base 775: `ef4b2370dacf85db74df9f37e82a37c76dd584b2`
(`feature/styleos-native-desktop-launch-775`, GitHub Actions 44952900969 PASS).
Stage branch: `feature/styleos-native-disk-staging-776`.

**Do not call 776 accepted until its final commit, GitHub Actions success,
matching CI source/ISO artifacts and SHA-256 are verified.** First read
`AGENTS.md`, `docs/PROJECT.md`, `docs/architecture/NATIVE_DESKTOP_ADMISSION_775.md`,
`docs/architecture/NATIVE_DISK_TEA_STAGING_776.md` and 768–774 reports.

Stage 776 unifies the read-only disk→pinned canonical `.tea`→single ELF64 code
page step as `nts_prepare`. In the dedicated experimental 769 path, it reads
real IDE sectors, prints an explicit 776 marker, and continues through actual
CPL3 execution. The same source compiles into the normal Desktop but remains
dormant: it DOES NOT itself create the user process, allocate memory, switch
CR3, call `ndl_admit`, render a native app, authenticate users or implement
full `.tea` installation. Host ASAN/UBSAN and real QEMU tests are separate.

**Next stage 777** should advance the hardest remaining gap: trusted physical
page ownership and complete x86-64 SAFE kernel mapping, then a bounded bridge
from disk ELF staging to the pre-dispatch 775 coordinator. Do not activate
in the regular Desktop until verified by independent positive/negative
hardware tests. Full acceptance remains an app running simultaneously with
Desktop: real PID, own CPL3 root, returning syscall, IPC, independent window,
input, EXIT/FAULT cleanup and shell survival. One small safe change per branch.

After independent apps: user login/chooser per approved mockup, real sessions
and authentication, StyleFS separate homes/access rights/recovery, then final
Explorer according to approved mockups, keeping `Быстрый доступ`. Preserve
current more elaborate Desktop. Never update `main` or old accepted branches
without authorization.

For each new stage deliver exact source ZIP, verified bootable ISO, QEMU logs,
GitHub commit/run and a new-chat continuation with honest scope.
