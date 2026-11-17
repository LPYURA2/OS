# StyleOS T-P1.8B — результат и свидетельства

[Полный технический отчёт на русском](REPORT_RU.md) · [ADR-0012](../../adr/0012-external-disk-app.md) · [воспроизведение](../../TESTING.md#внешняя-программа-с-диска-t-p18b)

Проверенный код: `0ef43423de1fc4820c7efece9c3da286879d7b49`, база: `1ea8d090cd12c4a5d20f628c927fcba787aede98`.
Публикуется только `feature/styleos-t-p1-8b`; main и Portable-os не изменяются.

| Проверка Linux | Фактический результат |
|---|---|
| Внешнее приложение: A, B и отказ loader | **16/16 PASS**, exit 0, чистый кодовый коммит |
| Прежний полный набор / ARM-only | **57/57 / 43/43 PASS**, по 296 Python tests |
| Прежний storage runner | **16/16 PASS**, 39 Python tests и семь VM-фаз |
| Бинарная регрессия | **46/46** артефактов идентичны базе |
| Python 3.14, `os.link` недоступен | 70 tests: 69 PASS, один hardlink-case SKIPPED |
| Новый Android/Termux, VirtualBox, native hardware | **SKIPPED**: ещё не испытаны |

Одинаковый boot image читает разные внешние файлы с новых дисков. После настоящего EL0 fault происходит свежий вход в EL0 и выход с результатом payload. Файл с неверным magic отклоняется до копирования/EL0. Диски после VM не изменились. Это закрытый AArch64 QEMU-тест, не общая платформа приложений.

Основные файлы свидетельств:

- `new-run/`: полный финальный runner, provenance, manifest, independent sites, app metadata, UART/JSON трёх VM.
- `full-regression/`, `arm-regression/`, `storage-regression/`: прежние проверки после изменений.
- `baseline/`: те же проверки точной базы на том же toolchain.
- `artifact-comparison.json`, `tested-sources.json`, `scope-audit.json`, `protected-base-files.json`: SHA и сохранность кода/истории.
- `final-review.md/json`, `safety-audit.md/json`, `audit/`: независимые проверки и их границы.
- `environment.json`, `*-console.log`: версии и команды; `SHA256SUMS`: контрольные суммы всех файлов этого каталога.

Сырые `.uart` скопированы побайтно с расширением `.uart.log` для сохранения CRLF через Git; их SHA в JSON не меняются. В Git находятся только текстовые свидетельства. Диски, ELF, raw payload и boot images остаются вне Git в build-каталогах.

```sh
cd docs/test-results/t-p1-8b
sha256sum -c SHA256SUMS
```

Для повторного нового прогона в Termux после checkout кодового SHA:

```sh
CC=clang python tools/test_disk_app.py --output build/termux-tp18b
```

Предыдущий результат владельца T-P1.8A (16/16 PASS, SHA60c9a18) записан отдельно в отчёте и не является Android T-P1.8B PASS. После публикации этой ветки работа остановлена; следующий этап требует отдельного согласования.
