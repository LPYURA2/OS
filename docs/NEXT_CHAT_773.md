# StyleOS — continuation after stage 773

Repository: LPYURA2/OS. Accepted base stage 772:
`d9d6f3c2f0d869f74d4499351f8757ddda196a5b`, CI 44947257064 PASS.

Current task branch: `feature/styleos-returning-cpl3-window-ipc-773`.
**Verify the final branch commit and CI outcome yourself** before accepting
stage 773; documentation does not substitute for a passing hardware run.

Technical files: `arch/x86_64/user_processes.c`,
`arch/x86_64/user_processes_entry.S`,
`include/styleos/user_ipc_773.h`, `kernel/user_ipc_773.c`,
`kernel/user_processes_demo.c`, `tools/test_user_ipc_773.py`,
`tools/test_user_ipc_773_guest.py`, and
`docs/architecture/CPL3_RETURNING_WINDOW_IPC_773.md`.

Scope: versioned bounded request and kernel-to-user reply over genuine CPL3
INT80/IRETQ in a dedicated x86 QEMU profile. User A's actual PTE-backed stack
and hardware-selected CR3 are checked before native_window_channel receives a
prearmed key event. Negative test refuses foreign reply VA without consuming
it, retries and then completes the established 4-event fault-isolation path.
No production Desktop syscall, general allocator or graphical `.tea` output.

Next: stage 774 in a NEW branch. Integrate process lifetime, safe shared
surfaces and real `.tea` launch into the production Desktop one checkable
slice at a time. Keep existing UI unchanged, never invent PID/CPU/RAM.
Following actual independent apps: approved login chooser and account isolation,
StyleFS user homes/permissions/recovery, final Explorer by approved mockups.

Workflow: no main changes, no old branches rewritten or deleted. For every
accepted stage provide exact-commit full source ZIP, QEMU-PASS ISO, evidence
archive and new-chat continuation. Prefer test-only profiles until real owned
physical address spaces exist. Any failed CI means not accepted.
