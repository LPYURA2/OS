# Native StyleOS applications: .tea only

Owner decision (21 Nov 2026): **every system and third-party native app is a standalone .tea package and must ultimately execute in its own real user-mode process**. The shell owns the desktop, compositor, window management, input routing and app launch requests, not Documents/Notes/Editor logic, state, parsers or file access.

Native package v1 (host tooling already implemented) is a reproducible ZIP with exactly two stored members: manifest.json (up to 4096 bytes) and bin/x86_64/app.elf or bin/aarch64/app.elf (ELF64 LE, up to 16 MiB). Manifest includes format styleos.tea, schema_version 1, app id, name, version, architecture, entry path, SHA256 and permissions [] by default. Unsafe paths, executable RWX load segments, unsigned permissions and invalid ELF are refused by tools/tea_package.py, tested by tools/test_tea_package.py.

**Scope warning**: a host package tool is not the native guest launcher or isolation. SHA256 here is not a cryptographic signature or trust root. Current shell still contains app UI code and current F5 PID is only a session ID. Complete separation requires:

A2: kernel app registry, ELF loader with real x86-64 CPL3 address spaces, syscall/IPC ABI, permissions and fault isolation.
A3: independent window surfaces and IPC so F10 restarts only the shell; apps preserve actual PIDs, memory and unsaved drafts.
A4: migrate Editor.tea, Notes.tea, Explorer.tea, Processes.tea and Documents.tea out of shell; each has an isolated user-mode process and storage grants.
A5: native package installation and preinstalled system app registry, verified in QEMU/OVMF with app running outside kernel. Until then **full separation NOT DONE**.

Android APK and Windows EXE will be system-provided compatibility components, *not* native .tea or separate emulation-app windows. Legacy .sapp is historical only, never a new native alternative.

Visual constraints: current desktop and palette unchanged; Explorer strictly follows the approved 20-screen PDF; Documents strictly follows approved 14-screen PDF.