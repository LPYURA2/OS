# Transition: app session names now owned by native catalog

Previous stable guest: b97a1d6f. Nine mandatory native system app IDs are defined in installer/system-apps.json and kernel/system_apps.c.

The existing F8 session panel now obtains app display names from the single native system-app catalog, by stable org.styleos.* IDs; desktop_shell.c, rendering code, colors, sizes, keyboard handling and existing window layouts are unchanged.

IMPORTANT: these remain five HISTORICAL session slots, NOT five real user-mode processes, and the remaining four native first-release app entries are a contract, not installed/launchable programs. Do not claim full .tea separation before CPL3/ELF/IPC and installation acceptance.

Host tests assert IDs and names, existing QEMU F8/F10 tests remain mandatory. Next: actual guest process loader and filesystem/window broker, then replace legacy sessions.
