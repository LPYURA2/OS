# StyleOS — continuation point after stage 769

Repository: LPYURA2/OS. Intended accepted branch
`feature/styleos-guest-disk-tea-cpl3-769`; **verify exact SHA and CI conclusion
from GitHub on entry** before declaring the stage accepted.

Read AGENTS.md, docs/PROJECT.md, docs/architecture/NATIVE_TEA_GUEST_DISK_769.md,
NATIVE_TEA_ELF_CPL3_768.md and NATIVE_LAUNCH_TRANSACTION_767.md.

Stages 759–768: read-only kernel PID snapshots, bounded hardware dispatch,
desktop kernel service, strict frame/root policies, user session registry,
launch-transaction rollback, and physically verified one-page x86 CPL3 ELF
from a build-embedded .tea. All are limited technical increments, NOT a
complete multiuser OS or native app manager.

Stage 769: on-guest **external disk-sourced** canonical pinned `.tea`, ZIP
verification and ELF loading; real CPL3 trap sequence in QEMU, separate
mutated disk rejection before PID/entry. Experimental x86 i440fx IDE only,
no Desktop integration or general user-space file driver.

NEXT 770 (if stage 769 accepted): design kernel-trusted dispatch from guest
package verification into 767 transaction with actual PID, session, window;
independent lifecycle and error rollback. Start with a dedicated test profile,
not ordinary Desktop. Do not display fake CPU/RAM or mix UI session IDs/PIDs.

Owner rules: one stage per NEW branch; do not touch main, original visuals,
second data disk or accepted checkpoints. Each result must include exact CI
full ZIP source, QEMU PASS Desktop ISO, test evidence and next-chat note.
Login screen and user selection later strictly by approved mockups, after
real credentials and storage isolation have a verified foundation.
