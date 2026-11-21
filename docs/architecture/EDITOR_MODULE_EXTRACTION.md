# First extraction of Editor/Notes code from desktop shell

Base: 8a3affda. Independent text mutation now lives in kernel/desktop_text_app.c with public draft API in include/styleos/desktop_text_app.h. The shell only delegates to desk_text_app_replace. No display, layout, color, frame renderer, icon or wallpaper changes.

Host ASAN/UBSAN tests cover independent Notes and Editor UTF-8 drafts, editing and limits; QEMU F3/F5/F10 regression is mandatory and must be checked in Actions. This is **a source-level extraction, not a user-mode process**. Other editing, file operations and rendering still remain in the shell until the real .tea loader and IPC are available.
