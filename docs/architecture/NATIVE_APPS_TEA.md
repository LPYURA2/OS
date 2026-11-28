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

## Required common Deep Drop SDK integration
Owner decision (28 Nov 2026): future independently installed .tea applications must be able to participate through a common versioned SDK/system service, without per-app modifications of the shell or kernel. See [DEEP_DROP_APP_CONTRACT.md](DEEP_DROP_APP_CONTRACT.md). Design real apps with selection/receive/insert/undo boundaries now; generalized capability negotiation, installation interoperability, streaming and full system-wide Deep Drop acceptance follow the app migration. Current pinned samples are not a production SDK or proof of this requirement.

## Stage 104 — generic boot-lifetime approvals (candidate)
A trusted supervisor registers immutable externally reviewed package, ELF and manifest pins in a bounded registry, with an approved display title and input-wait opt-in. Explorer resolves the selected exact StyleFS revision and validates CRC, package SHA-256, canonical ZIP, exact approved manifest and ELF digest before passing it to the existing isolated loader. Filename is not authority. No package can register itself. Existing built-in sample has a separately trusted boot pin; arbitrary publisher review/signature and durable installation are not implemented. Registry currently four external entries and 16 KiB package bound, legacy ELF limit 8192 bytes; actual application migration, larger packages, persistence and installer UI remain outstanding.

## Stage 105 — expanded package admission (candidate)
The live trusted registry and canonical ZIP parser accept packages up to 136 KiB and ELF up to 128 KiB, with a bounded supervisor scratch and used-byte scrubbing. Actual executable layout must fit the existing 8 RX, 16 RW and 1 stack page policy (expanded loader supports up to 4 stack pages in other contexts). The old raw-media parser and one-page reader retain 16 KiB and 8192-byte bounds respectively. The writable guest fixture now contains a 20 KiB ELF with initialized data on the second RW page, validating actual loaded data before exercising 8193-byte streams. Neither package size nor host format support means arbitrary executables or a production installer. Persistent approval and user review remain to be implemented.
