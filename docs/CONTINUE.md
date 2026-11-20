# StyleOS — точка продолжения после этапа 757

**Контрольная сборка:** 744, ISO SHA256 `011dd3560e8d232b00201623443d9a09b0c67e03227b66504312a31f75e285ae`.
Ничего из системы 744 не перезаписано и не объявлено восстановленным.

**Последняя опубликованная база:** `feature/styleos-documents-edit-756`, `899f826658195bb9a76ffbc59a8e13da3ed84c0c`.
**Новая ветка:** `feature/styleos-system-app-preinstall-757`.

**Утверждённое продуктом:** «Документы» входят в стандартную установку StyleOS,
но исполняются как самостоятельное системное приложение `.tea` отдельным
процессом, не как часть desktop shell. Не требуют App World. Рабочий стол
управляет окном и запуском; приложение — своими функциями и файлом.

**Реальные результаты:** независимый ELF `Documents.tea` с ограниченными
операциями TXT/DOCX, поиском/заменой и undo/redo (755–756); разработческий
host provisioning contract (757): preinstall manifest, регистрация на system
volume, атомарное поколение, rollback, контроль целостности, generic ELF
validator и deterministic ZIP. Test scripts: `sh tools/test_system_apps_757.sh`.

**BLOCKERS:** Стандартный native installer 744 и загрузчик не поддерживают
новый ABI; подписей нет, и production установка специально блокируется.
Сквозного запуска в StyleOS нет, ISO новой версии нет; полного GUI по
14 макетам нет. Host ZIP dev не считать ни ISO, ни работающим installed app.

**Следующий этап:** native host→guest system registry/install bridge,
настоящий ELF user process loader + брокер окон и файлов, интеграция
с актуальной ОС, verified guest-QEMU. Параллельно восстановить весь Documents
GUI/сервисные функции и DOCX. Не смешивать код редактора с рабочим столом.
Промежуточные исходники и артефакты всегда публиковать в новой ветке и ZIP.
