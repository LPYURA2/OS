# TASK-001 — Establish a safe development loop for MATRYOSHKA

**Goal:** Prepare existing MATRYOSHKA Termux v0.3.1 for repeatable, test-driven development. Do not rewrite the guest OS.

1. Read `AGENTS.md`, the unified passport, `README_RU.md`, `ARCHITECTURE.md`, `docs/TERMUX_PLAN.md`, `docs/ADR_001_ISA.md`.
2. Verify the baseline commands:
   - `python matryoshka.py --selftest`
   - `python tools/export_images.py --check`
3. Add a simple `python`-only test runner/script and optionally GitHub Actions CI on Linux, with deterministic exit codes; avoid introducing dependencies unless necessary.
4. Review coverage and add tests if missing for boot image integrity/header rejection, persistent disk save/read across reboot, and no regression of guest `help/ps/info` commands. Keep personal disk images untouched.
5. Do not change the semantics of CPU instructions, boot firmware, kernel, shell, system image format, guest ABI, devices, or persistent user disk format. If an existing defect requires such a change, stop and document it for approval instead of silently changing the architecture.
6. Update `CHANGELOG.md`, `docs/PROJECT_STATUS.md`, and the passport's task/change log accurately. Distinguish tests actually run from assumptions.
7. Output an isolated feature branch + pull request, modified-file list, test logs, precise Termux verification commands, regression risks, and rollback instructions.

**Acceptance:** Linux CI/baseline passes, no changes to guest-executed binaries or disk format; ready for owner-run Android/Termux check. Merging/releasing requires explicit owner approval.

**Next task, not included here:** design and implement guest-side loading of a test application from `system.img`, which today is handled by Python. Do not start this next task automatically.
