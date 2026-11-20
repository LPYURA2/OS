# StyleOS Documents — recovery sources (stage 757)

**Current owner-approved requirement:** Documents is preinstalled by the StyleOS
installer as a standalone SYSTEM APPLICATION (`Documents.tea`). It must never
be compiled into the desktop shell or the kernel. The desktop launches it and
brokers windows/files; the app owns the editor and its document state.

There is still **no installer-compatible bootable ISO** and the existing
StyleOS 744 loader does not support the experimental `.tea` ABI. Production
preinstallation intentionally fails until signing and native loader exist.

New code for stage 757:

- `installer/system-apps.json` and `installer/README.md`: first-party default
  application plan and system-vs-shell boundaries;
- `tools/tea_package.py`: reusable host ELF policy validator;
- `tools/system_apps.py`: development-only reference system-volume provisioner,
  independent of the Documents builder;
- `tools/build_preinstall_bundle.py`: content-addressed deterministic
  preinstall development bundle (NOT BOOTABLE);
- `tests/test_system_apps.py`, `tests/test_tea_package.py`,
  `tests/test_preinstall_bundle.py`: refusal, identity, tamper,
  rollback, user-data isolation and reproducible bundle tests.

Previous stages 752–756 remain present in this source tree; see
`docs/CONTINUE.md` and `docs/RECOVERY_757_RU.md`.

```
sh tools/test_system_apps_757.sh
```

No user data disks or original 744 images are modified by these tests.
