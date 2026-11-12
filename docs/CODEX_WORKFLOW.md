# MATRYOSHKA OS — GitHub / Codex / Termux

## 1. Repository
Create a private GitHub repository named `OS` and upload the contents of this ZIP into its root (not the ZIP itself). Tag the untouched initial source `baseline-v0.3.1`; changes from this starter package are limited to documentation and GitHub workflow files.

Suggested Git commands (when the repository is empty):
```sh
git init -b main
git add .
git commit -m "Baseline MATRYOSHKA Termux v0.3.1 + project docs"
git remote add origin YOUR_REPOSITORY_URL
git push -u origin main
git tag baseline-v0.3.1
git push origin baseline-v0.3.1
```

Do not push your real `matryoshka_user_disk.img`. Keep a separate backup of it and of the initial zip.

## 2. Connect Codex
Prepare a Codex Cloud environment with this GitHub repository (setup on the web/desktop, then available on supported mobile devices). Point Codex to `AGENTS.md` and `docs/TASK_001_CODEX.md`.

## 3. Change control
- Each task must be a branch/PR and have regression tests.
- `main` remains the last accepted runnable version.
- Never let CI or an agent overwrite personal Termux user data.
- After PR review run the exact version on the Android device; provide logs and OS screenshots.
- Once approved, merge, tag new version, publish a ZIP from the repository, and update the unified passport.

## 4. Test on Termux
```sh
pkg install python git -y
cd ~/OS
python matryoshka.py --selftest
python tools/export_images.py --check
python matryoshka.py --os
```
In the guest shell: `help`, `ps`, `info`, `save Test`, `ls`, `load`, `exit`. Restart `--os` and run `load` again.

## 5. Boundaries
The Termux baseline is still experimental M32; RV32I was discussed as a candidate, not approved. Proteus, Limbo, and ARM64 are later independent phases. APK/EXE/.tea support is a goal, not an implemented feature.
