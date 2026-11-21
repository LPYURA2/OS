# Restore 730 behavior: F10 shell rebuild with persistent editor draft

Original reference: the immutable `StyleOS-730-Test.zip` (build 730, tested by owner).
Its `START.txt` specifies F3 editor -> type without save -> F10 rebuild -> Ctrl+S save -> reboot -> reopen. The same document calls for stable process IDs across F10. In recovered 709 the editor draft was still a set of globals in `kernel/desktop_shell.c` and there was no F10 handling.

This is **newly reconstructed code**, NOT a byte-identical recovery of the original 730 C sources.

## What this stage actually does

- Moves text buffer, incoming buffer, caret, file id, folder and dirty flag to `kernel/desktop_editor_runtime.c`. This state does not belong to window geometry and is initialized once per boot.
- Adds F10 keyboard handling in the shell. F10 restores default window geometry and transient input state, without remounting or writing user disks, resetting draft or restarting the machine.
- Keeps window open/minimized state, and forces a complete framebuffer repaint after rebuilding.
- Adds a host test that reopens the same editor runtime and confirms the dirty text, cursor, folder and document survive.
- Extends the existing QEMU end-to-end test to type a draft, use F10 before saving, save, reboot and reopen.

## Important scope boundary

This **does not** yet recreate original 730 independent native processes or prove PID stability. A persistent editor runtime object is not an isolated process; `.tea` app loading is separate future integration. No other application is silently renamed or claimed recovered.

## Evidence

Local Clang/LLD x86_64 EFI compilation PASS. Local editor session host test PASS. Prior host checks 10/10 PASS. QEMU result must come from the real `restore-730-f10-state.yml` CI run; never infer it from local compilation alone.

Restore base: `f81631d980824f2338ae68eaca0a7c549cc8161f` (709), full source archived as `StyleOS-709-FULL-RECOVERED-SOURCE.zip`. Control 730 and 744 remain unchanged.
