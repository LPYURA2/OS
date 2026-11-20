# System applications: installer-facing contract (development)

`system-apps.json` lists first-party apps included at first installation.
`styleos.documents` is preinstalled by default, in the **system volume**, as
an **independent `.tea` package**; it must not be compiled into the desktop.

The *generic* `tools/system_apps.py` reference provisioner accepts a verified
`.tea` and writes to the target system root:

```
System/Applications/current
System/Applications/releases/<sha256-generation>/catalog.json
System/Applications/releases/<sha256-generation>/packages/Documents.tea
```

`current` is a single atomic generation pointer. A new generation is written
and verified before it becomes current; the prior generation remains for
rollback. The tool must not touch `/Users`, the home directory, a data disk,
or the 744 ISO. A production native installer shall use the equivalent
transaction on the actual StyleOS system volume and offer the app immediately
after installation (no App World download). The shell discovers app records
through **one system-wide registry service**, rather than a hardcoded window
or link against the app.

The `Documents.tea` package uses experimental ABI
`styleos.documents-preview-v1`. This ABI does not exist in 744, and publisher
signatures are not implemented. Therefore `stage --profile production` is
intentionally rejected. A development host staging passes only with the
explicit `--profile development` flag. A host disk folder and ZIP bundle are
not a bootable image and do not prove app launch in a guest OS.

## Reproduce (on Linux/macOS with Python 3)

```
python3 tools/build_documents_tea.py --output build/756/Documents.tea
python3 tools/build_preinstall_bundle.py build \
 --tea build/756/Documents.tea \
 --output build/757/StyleOS-Documents-System-Preinstall-DEV.zip
mkdir -p /tmp/styleos-system-volume
python3 tools/build_preinstall_bundle.py stage-bundle \
 --bundle build/757/StyleOS-Documents-System-Preinstall-DEV.zip \
 --root /tmp/styleos-system-volume --profile development
python3 tools/system_apps.py verify --root /tmp/styleos-system-volume
```

Keep the real system disk unmounted: this is only a disposable host-test root.

## Ownership and isolation

- **Installer:** chooses included system applications and provisions their
  package + catalog in the system volume, before first user login.
- **System application registry:** exposes ID, label, version, entry contract,
  immutable package hash and launch policy. It does not execute any bytes.
- **App process manager/ELF loader (not yet implemented):** checks the approved
  versioned ABI, signatures, isolation and memory mappings, then creates an
  independent user-mode process.
- **Desktop shell:** obtains menu entries and asks the process manager to
  open an app. It does not implement document editing or access app internals.
- **Window/file broker:** owns window chrome and user-granted file tokens.
  The app draws only in its client area and cannot read arbitrary paths.
- **Documents.tea:** owns editor UI, text model, DOCX/TXT and document state.
  Updates to it are independent from desktop/kernel releases.

**Do not** call the current `sha256` a digital signature. Signature trust,
versioned ABI, native loader, and complete document functionality are required
before enabling the production path.
