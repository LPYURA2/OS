# StyleOS — точка продолжения после этапа 764

Project: `LPYURA2/OS` (NOT `Portable-os`). Date: 21.11.2026.

## Стартовая инструкция новому чату

> Продолжай разработку StyleOS из **последней принятой CI-ветки**.
> Прочитай `AGENTS.md`, `docs/PROJECT.md`, `docs/architecture/DESKTOP_FRAME_OWNERSHIP_764.md`,
> `docs/architecture/DESKTOP_VM_STAGE_762.md` и
> `docs/architecture/DESKTOP_CR3_ROUNDTRIP_763.md`.
> Проверь точный коммит, SHA, текущий CI и исходники, не полагайся на устные
> отчёты. Каждый следующий этап — новая ветка, без merge в `main`.
> Перед результатом дай три артефакта: **полный архив исходников,
> загрузочный ISO из успешного CI, обновлённую точку продолжения**.
> Не нарушай существующий рабочий стол, .tea, файлы второго диска и
> восстановительные снимки 709/730.

## Принято ранее

- 759: read-only snapshot API for kernel process manager — CI PASS.
- 760: two real test CPL3/EL0 contexts, scheduler-backed PID — CI PASS.
- 761: empty desktop PID registry bootstrapped from real x86 CR3 — CI PASS.
- 762: dormant SAFE + two user table builder, W^X, alias refusal — CI PASS.
- 763: separate hardware test of old image-owned PG CR3 → restore — CI PASS,
  base commit `e7258f11607c271a1e842182697d3543a541bc78`.
- 764: new branch `feature/styleos-frame-ownership-764`, bounded ownership
  ledger for *pre-admitted* frames, registry PID binding and guarded teardown.
  **Check the CI run of exact branch commit before marking 764 accepted.**

## What is still NOT implemented

- No trusted physical grants for the new 762/764 roots; no general PMM or
  preemptive desktop process scheduler; no hardware CR3 switch for 762 roots.
- No independent `.tea` execution in the ordinary desktop; no user-mode
  process-info syscall; no real CPU%/RAM telemetry.
- No real multi-user accounts, identity isolation or login screen yet.
- 730 binary surviving is not proof all lost post-709 sources were recovered.
- VirtualBox proof is separate from QEMU proof; no claim of hardware support.

## Next 765 — grounded acceptance

Create a new branch from accepted 764, build an x86-64 UEFI **physical-grant
attestation** profile with real page-table walk and reserved-image ownership
proof before any candidate-root activation. Require negative tests for
unreserved/candidate/overlapping frames and incomplete kernel closure.
Only then begin dedicated QEMU activation, never directly in the ordinary
UI Desktop. Later connect real `.tea` lifecycle, `Processes.tea`, and user
sessions/login exactly to approved mockups.

**Per-stage delivery rule:** new branch, verified exact SHA, test evidence,
GitHub Actions artifacts, full source ZIP, bootable ISO, this continuation
file. Do not erase older checkpoints or switch `main`.
