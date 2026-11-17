# T-P1.8A — исправление host-тестов для Termux / Python 3.14

Исправление: [`60c9a18fd6b3c23865d64ca5e544faf3a23c6bbf`](https://github.com/LPYURA2/OS/commit/60c9a18fd6b3c23865d64ca5e544faf3a23c6bbf).
База ветки — `2e375cc122759ac4d4ab5f1be44cf2be7ce54f0d`; владелец обнаружил
две ошибки `host-storage-python` при контроле реализации `6dfb61d…` в
Android/Termux, Python 3.14, QEMU 11.1.3. Это сообщение об ошибках, а не PASS
телефонного прогона. Новый контроль Android ещё не выполнен здесь.

## Изменение

Кодовый коммит меняет только [tests/test_storage_runner.py](../../../tests/test_storage_runner.py).
Подмены `check_output`, `run` и `which` перехватывают точные fixture-команды
Git/QEMU, прочие вызовы передают сохранённым настоящим функциям. Типы ответов
Git сохранены: HEAD — text, status — bytes. Обнаружение версий инструментов
изолировано отдельно. Тест принудительно вызывает путь `_syscmd_file`, чтобы
кэш `platform()` не скрывал повторение ошибки.

Hardlink-проверка выделена в самостоятельный тест. Только она получает
SKIPPED при отсутствующем `os.link`, NotImplementedError либо явно
неподдерживаемой/запрещённой операции ENOSYS/EOPNOTSUPP/EPERM при создании
ссылки. Остальные ошибки, например EIO, остаются ошибками. Проверки inode,
размера, обычного файла и symlink обязательны. Рабочая проверка `st_nlink`
в раннере не изменена.

Ядро, драйвер, раннер, формат диска и C-тесты не менялись. Второй отдельный
коммит добавляет этот отчёт и обновляет SHA команды в TESTING; его SHA указан
в итоговом сообщении владельцу. Исторические свидетельства сохранены.

## Фактические проверки, Linux

| Проверка | Результат | Свидетельство |
|---|---|---|
| Обе исходные ошибки | Воспроизведены до исправления | [reproduced-before.log](reproduced-before.log): два ожидаемых ERROR |
| Storage Python 3.13 | PASS, 39 tests, OK | [журнал](python313-storage.log) |
| Storage Python 3.14.10 | PASS, 39 tests, OK | [журнал](python314-storage.log) |
| Python 3.14.10 без атрибута `os.link` | PASS: 38 прошли, только hardlink SKIPPED | [журнал](python314-without-os-link.log) |
| Принудительный `_syscmd_file`, unsupported link, неожиданный EIO | PASS: совместимость и граница skip подтверждены | [сводка](compatibility-simulation.json), [журнал](compatibility-simulation.log) |
| Полный регрессионный набор на Python 3.14.10 | PASS, 57/57, exit 0 | [results](regression-full/results.json), [консоль](regression-full/console.log) |
| ARM64-only на Python 3.14.10 без OVMF/x86 tools | PASS, 43/43, exit 0 | [results](regression-arm-only/results.json), [консоль](regression-arm-only/console.log) |
| Python unittest обоих регрессионных наборов | PASS, 262 tests, OK | [полный](regression-full/host-python.log), [ARM-only](regression-arm-only/host-python.log) |
| Одна storage-команда на закреплённом SHA | PASS, 16/16, exit 0 | [results](storage/test-results/results.json), [консоль](storage/console.log), [provenance](storage/provenance.json) |
| Гостевые бинарники | PASS, 46/46 идентичны прежним | [artifact-integrity.json](artifact-integrity.json): 44 прежних + storage ELF/Image |
| Повторный Android/Termux, Python 3.14/QEMU 11.1.3 | SKIPPED | Требуется контроль владельца нового SHA |

В simulation log последний ERROR — намеренная проверка, что неожиданный EIO
не превращается в skip; весь simulation script завершился успешно. В обычных
финальных наборах ошибок нет. Число Python-тестов выросло на один после
разделения hardlink-кейса; остальные проверки не отключены.

Storage-команда выполнена с clean Git HEAD исправления. Семь новых процессов
QEMU подтвердили запись/чтение A, обновление/чтение B и восстановление A после
подтверждённого барьера и SIGKILL до commit. Все UART/JSON сохранены в
[storage/test-results](storage/test-results/); `.uart.log` содержит исходные
UART-байты с SHA из JSON. C-модели также прошли. Формат и протокол трёх FLUSH
сохранены: прежний [отчёт T-P1.8A](../t-p1-8a/README.md).

Проверялось в Linux x86-64: Python 3.14.10, Clang/LLD 19.1.7, QEMU 10.0.13.
Python 3.14 установлен только во внешнем пользовательском scratch-каталоге;
runtime не включён в Git. [Среда и команды](environment.json),
[граница изменения](implementation-scope.json), [110 source hashes](tested-sources.json).
Здесь опубликованы выбранные регрессионные журналы; полный консольный итог
содержит все 57/43 этапа. Linux и имитация отсутствующего API не заменяют
фактический Android-прогон.

## Повтор в Termux

В имеющемся клоне OS:

```sh
git fetch origin feature/styleos-t-p1-8a
git checkout --detach 60c9a18fd6b3c23865d64ca5e544faf3a23c6bbf
git rev-parse HEAD
CC=clang python tools/test_storage.py --output build/termux-tp18a
echo $?
```

Команда сама создаёт новый run-каталог, сохраняя старые диски и логи.
При отсутствии `os.link` ожидается один явно обозначенный SKIPPED внутри
`host-storage-python`, без отключения остальных тестов. Сохраните выведенный
RUN path, `provenance.json`, `run.json` и все `test-results/*.log`, `*.json` и `*.uart`.

Проверка свидетельств: `sha256sum -c SHA256SUMS` из этого каталога.
Публикуется обычным fast-forward только `feature/styleos-t-p1-8a`.
`main`, Portable-os, прочие refs и старые отчёты не изменяются. Force push,
merge и T-P1.8B не выполняются. После публикации работа останавливается.
