# S837 — same-boot detached plan composition

Accepted S836 ce28f4dda06904d4a9bc87301cdc2c0511ef97c2, CI 45407724842 SUCCESS (182 PASS, 5 SKIPPED). Dedicated --test-guest-plan-837 combines existing read-only ownership and S835 diagnostics on the image-owned stack. The runner rebuilds binary-site metadata, verifies the full prior proof and compares the composed map to independent same-boot evidence. Existing terminal profiles remain separate. No allocation or new grants; no production integration. Preserve complete logs, QMP traces, binary-site proof, image and manifest.

Local: four changed C modules compiled; 16/16 composition tests, 48/48 provenance/profile-refusal tests on original S835 maps with synthetic live records; four ordinary/Desktop binaries identical to S836. Full ACPICA EFI link and genuine S837 QEMU remain untested locally. The earlier mock process timeout did not reproduce. Full CI and independent raw replay required before acceptance.
