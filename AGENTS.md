# Instructions for coding agents — MATRYOSHKA OS

The owner has final approval on architecture and releases. Treat current source as a working research prototype, not a mature OS.

## Read first
- `MATRYOSHKA_RAZRYV_OS_MASTER.docx`
- `README_RU.md`
- `ARCHITECTURE.md`
- `docs/TERMUX_PLAN.md`
- `docs/ADR_001_ISA.md`
- `docs/PROJECT_STATUS.md`

## Invariants
1. Guest OS logic must execute as guest CPU machine instructions. Never claim that Python host helpers are guest OS features.
2. Preserve the tested v0.3.1 baseline and T1A v0.3.2 source separately. Avoid rewrites and broad refactoring without authorization.
3. Never unilaterally change the existing ISA, existing syscall numbers, memory map, or persistent user disk format. T1A extends the experimental M32 ABI with catalog MAPP and syscall 10; future changes require owner approval.
4. `images/system.img` is a read-only boot/system image; `matryoshka_user_disk.img` is user data. Never overwrite personal data or commit user disks.
5. No destructive shell commands and no secrets in the repo. Respect the Termux user's other projects.
6. No root permissions or Android-side changes outside the project directory.
7. Report test commands and evidence, limitations, and a rollback plan with every PR. Tests passed in Linux do not prove Android compatibility.
8. Do not merge your own PR or tag a release without the owner's explicit approval.

## Baseline tests
```sh
python matryoshka.py --selftest
python tools/export_images.py --check
python -m unittest discover -s tests -v
```

## Development workflow
Use a narrow `feature/...` branch and a PR; update `CHANGELOG.md` and the passport with confirmed changes only. Honor the current Codex task file in `docs/TASK_001_CODEX.md`.

## Product vision and learning obligations (2026-11-12)
- Before proposing new architecture read `docs/PRODUCT_VISION_RU.md`, `docs/QUALITY_GATES_RU.md`, and `docs/CURRENT_BASELINE_RU.md`.
- MATRYOSHKA aims to be a distinct competitive OS alternative, not a Linux or Windows clone. Compatibility goals APK/EXE/.tea are unimplemented.
- Always separate guest OS functions from host engine demos, and prototype, verified Android results, and product aspirations.
- For each major change provide a short technical explanation and one learn-by-doing exercise for the owner.
- T1B console repair is only a proposal pending architecture approval; see `docs/TASK_002_T1B_CODEX.md`.
