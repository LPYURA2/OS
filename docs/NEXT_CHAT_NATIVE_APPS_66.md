# Ordinary shell provider 66 candidate

Accepted 65 6cba5d7195026110efe2a12d0c08326b5f7c2958, CI 46041382056 4/4 SUCCESS. Owner order: independent applications acceptance -> Deep Drop -> system apps -> network/browser/updates -> Beta -> sound/VPN. Preserve main and previous versions.

66 generates a normal separate controller ELF and pinned SHA-256 during ordinary owned-shell build. Immutable read-only image is embedded as data in the system image, not linked as privileged executable code. Uses audited normal loader and distinct CPL3 context. No diagnostic catalogue, no epoch-specific fault payload, no disk source or arbitrary user code. Registered before the supervisor loop; launches on actual F10 only, reads event, confirms exact epoch, exits. Existing diagnostics retain their independent fault/timeout provider. New ordinary ISO CI verifies actual QMP F10, deferred application, raw CPL3 syscall sequence, retirement and legacy UI regressions.

CI pending. This is a normal F10 control path, not all shell logic or full independence. Still need broader shell commands, separation of privileged legacy UI and built-in apps, and final acceptance. No UI redesign/merge/release. Deep Drop immediately after independence acceptance.
