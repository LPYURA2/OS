# StyleOS — точка продолжения после 771

Repo: `LPYURA2/OS`
Base accepted: 770 `3d8115d6a72b60d7fdf5ed2a2ce11cf5e847d6da`, CI `44944414807` PASS.
Stage branch: `feature/styleos-desktop-window-ipc-771`.
Latest stage SHA and CI outcome must be checked against GitHub before calling it
accepted; see the final user-facing report and branch GitHub Actions.

## Progress

- 768: dedicated QEMU profile executes verified ELF64 from a `.tea` in CPL3.
- 769: dedicated guest reads canonical `.tea` from an external IDE disk,
  rejects tampering before user execution.
- 770: real experimental user PIDs and page roots, two model user sessions,
  independent **kernel objects** for windows, fault/exit cleanup.
- 771: kernel-owned bounded native-window channel with isolated copied input
  events and an XRGB8888 16x16 tile staging buffer. The service starts empty
  in the **ordinary x86-64 Desktop**; host fixture tests PID/session/window
  gates. No live `.tea` startup or graphical separate windows in this desktop.

## Next 772

Implement and test an actual architecture-authenticated, copying CPL3 syscall
boundary in a dedicated controlled profile. No raw untrusted pointers into
kernel window state. Validate page translation, source/destination sizes,
syndrome/exception behavior, lifetime, session lock and failures. Only after
hardware validation wire real in-desktop `.tea` execution and rendering/input.

Then develop authenticated accounts and login selection **strictly according
to approved Library mockups**, file ACL/home directories on StyleFS/second
volume, then finish Explorer along approved mockups (`Быстрый доступ`, never
`Рекомендованное`). Preserve current rich desktop. No invented PID/CPU/RAM.

Each **new** stage gets a **new** GitHub branch, no main edits, no deleting old
sources/builds. Deliver full exact SHA source ZIP, CI-built ISO, test evidence
archive and next-chat Markdown. Read AGENTS.md / docs/PROJECT.md before edits.
