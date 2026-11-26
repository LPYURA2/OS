# Рабочая матрица приёмки ядра StyleOS

Область: x86-64 UEFI/OVMF, Q35 TCG, один vCPU; ARM64 — отдельная регрессия. Не релиз и не финальная приёмка. Предыдущий полностью зелёный интеграционный SHA 4b4dcf50a3da3cbbe54323f3f15de60c007ffb83, CI 45806269819 (19/19). Очищенный SHA 1a56b629e2f4633df4b314bea5897bd481f0a16f, CI 45807932074: сборка и проверка связанного кода дошли до проверки манифеста, затем Python UnboundLocalError (test_sources). Исправление только в проверяющем коде; результаты этого запуска не выдавать за PASS.

| Критерий | Имеющиеся свидетельства и обязательная проверка финальной ревизии | Статус |
|---|---|---|
| KFA-01 | Архив exact SHA, явные 93 единицы компиляции, отсутствие TEST_*; два независимых build с проверкой EFI/IMG/ISO и версий инструментов | OPEN |
| KFA-02 | Реальная загрузка owned root, QMP CR3/GDT/IDT/TR/CS; полная регрессия handoff и EBS | OPEN |
| KFA-03 | Guard/cache/root audits, W^X и негативные гостевые тесты; историческая полная регрессия | OPEN |
| KFA-04 | 96 страниц EFI arena, 32 пользовательских контекста; host-тесты отказов, reuse и scrubbing; не общая выдача всей RAM | OPEN |
| KFA-05 | Реальный LAPIC preemption в четырёхпроцессном профиле; повторить на финальной ревизии, короткий 32-процессный каталог не доказательство длительного IRQ | OPEN |
| KFA-06 | 32 реальных CPL3 в каталоге, четыре с FP/timer; подтвердить справедливость и длительность, не выдавать host ticks за аппаратные | OPEN |
| KFA-07 | Реальные INT80, PF/GP/UD и изоляция в разных профилях; независимые raw CPU evidence и отрицательные сценарии | OPEN |
| KFA-08 | Реальные fault/IST доказательства и отрицательные исторические проверки; fail-stop не восстановление ядра | OPEN |
| KFA-09 | Ограниченное Q35 hardware admission, DMA deny; исторические ACPICA, MCFG, DMAR/IVRS проверки отдельно, пропуски AMD не считать аппаратным PASS | OPEN |
| KFA-10 | Штатный ISO, сохранение UI, ввод, save/reopen и foreign-volume отказ | OPEN |
| KFA-11 | 32 повторные mixed batches, повторные optical storage boots и 32-резидентный каталог; сверить ограничения, IPC и реальные IRQ | OPEN |
| KFA-12 | Полная историческая ветка, обязательный tools/test.py --all с обеими архитектурами, отсутствие обязательных пропусков, сырые артефакты | OPEN |

Ветка feature/styleos-kernel-final-acceptance запускает обе существующие группы проверок, включая ранее условные исторические сценарии до 863. Новая проверка воспроизводимости сравнивает независимый обычный билд с реально испытанным образом. Если какой-либо обязательный тест отсутствует, не проходит или пропущен, критерий остаётся OPEN. После результатов заполнить точный SHA, ссылки, числа и ограничения; не выдавать прежние результаты за доказательство текущей ревизии. Отдельно измерить производительность и системные требования; не обещать поддержку физического железа или SMP. FS, installer, UI не разрабатывать до принятия владельцем.

## Первые фактические результаты — 20fe2eaad8207a8fadde2ecc9e7f37ee78a8956d

- KFA-01: job 139869839166 SUCCESS, artifact 15109869053. Два независимых обычных build дали побайтово одинаковые EFI, IMG, ISO. Манифест содержит 93 исходных модуля, 0 STYLEOS_TEST_*; архив точного SHA/tree 15111170124 скачан, SHA256 подтверждён. ISO SHA256 f9a77e806cb3d22cb1ed9a3583a8badbad9f088563fda01e33ea9fdf4d9d67ed. Свидетельство PASS на этой ревизии, не переносить автоматически на новый SHA.
- KFA-02/10: обычный no-catalog ISO загружен, две независимые загрузки, сохранение 2 секторных записи и повторное открытие с 0 записей; чужая идентичность тома отвергнута. QMP и actual source provenance проверены тем же job. Статус остаётся OPEN до полной сверки обязательных сценариев.
- KFA-06/07/08: catalog acceptance profile (НЕ релизный ISO) дал 32 отдельных процесса и в каждом из двух boot 64 реальных CPL3 события (60 INT80, 4 UD), полное освобождение; в этом коротком тесте нет аппаратных timer IRQ. Integrated test profile artifact 15111091305: 249 записей CPU trace на каждую из двух загрузок; не приравнивать их к количеству процессов или IRQ. Остальные аппаратные, отрицательные, FP, endurance и reuse критерии требуют отдельной сверки.
- KFA-12: job 139869879166, tools/test.py --all: по журналу 57 PASS, 0 FAIL, 0 SKIPPED, оба QEMU архитектурных режима. Job FAILURE только на upload-artifact: путь logs/ не существует; Checks.logs использует test-results/. Исправлен точный путь, без ослабления тестов и без изменений C/ASM. Повторить и сохранить результаты. Исторический workflow 45808477646 на момент проверки дошёл до аппаратной preemption 843, не завершён.
- Все критерии кроме имеющего доказательство KFA-01 остаются OPEN до проверки exact-SHA. Итоговая оценка производительности и требования не измерены. Обычный ISO не содержит каталога, поэтому проверка внешних приложений относится к отдельно скомпилированному acceptance profile, а не к самостоятельному штатному запуску .tea. Не объявлять 1.0 или общую физическую совместимость. Main untouched.

## Final evidence review and two additional acceptance workloads
Exact previous SHA ff069408b08e682593a11e152f17e1540f363d76: integration 45809347468 completed 20/20, historical 45809347491 completed 220 successful steps and five old branch-status publishing steps skipped, baseline 57/57. Artifacts 15112565031 source, 15110624564 ordinary and catalog ISO, 15111555563 baseline, 15113335516 historical. All historical test scopes remain separate. Remaining explicit evidence gaps: KFA-05 real preemption across 32 resident processes in common ordinary executor (catalog admission separately built, no forced timer), KFA-10 extended actual no-catalog ISO shell. New CI job acceptance-native-completion runs both, even if first fails. No C/ASM changes; final acceptance remains OPEN pending actual results and exact-SHA evidence. Existing Q35/TCG one-vCPU admission, finite arena and ordinary launch entry not wired to .tea UI remain limitations. Do not conflate catalog with ordinary executable launch.
