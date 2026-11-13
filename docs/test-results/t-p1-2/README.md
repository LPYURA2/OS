# T-P1.2 — фактические проверки ранних исключений

Исходники реализации: [`b497cfd92a8f308360a86ef1d8dfd3594940dd56`](https://github.com/LPYURA2/OS/commit/b497cfd92a8f308360a86ef1d8dfd3594940dd56), база — `e4823f2eaf1bfbdca6ea7a93768b76a3b289523b`. Этот отчёт добавлен отдельным коммитом без изменения исходников. Рабочая ветка — `feature/styleos-t-p1-2`; main и Portable-os не изменяются.

Среда — **Linux x86-64**, Python 3.12.14, Clang/LLD 19.1.7, GCC 14.2.0, QEMU 10.0.13, OVMF 2025.02. Фактические версии — [versions.log](versions.log), время UTC, команды, ограничения среды и SHA-256 firmware — [environment.json](environment.json). Toolchain/firmware распакованы в пользовательском каталоге; их бинарники в Git не включены.

## Итог

| Прогон | Результат | Доказательство |
|---|---|---|
| Исходная T-P1.1 до изменений | PASS, 18/18 этапов | [Журнал](baseline/console.log), [JSON](baseline/results.json) |
| Полный T-P1.2 | **PASS, 23/23 этапа**, exit 0 | [results.json](results.json) |
| Host Python внутри полного набора | **PASS, 39/39 unittest** | [Журнал](host-python.log) |
| ARM64-only маршрут без OVMF/x86-инструментов, в Linux | **PASS, 16/16 этапов**, exit 0 | [JSON](arm-only/results.json) |
| Новый T-P1.2 на телефоне Android/Termux | **SKIPPED** | Нового запуска владельца нет |
| Физическая загрузка x86/ARM, VirtualBox | **SKIPPED** | Эти среды не испытывались в T-P1.2 |
| Реальные Data Abort и #PF новым обработчиком | **Не проверены** | Управляемые аппаратные сценарии — только BRK и #UD; классификация других причин проверена на хосте |

23 этапа — пять host-наборов, две повторные сборки и сравнение, четыре сборки вариантов и 11 QEMU-сценариев. Это не сумма независимых unit-тестов. Отдельные C-наборы и 39 Python unittest имеют собственное покрытие.

Полный фактически выполненный запуск:

```sh
python3 tools/test.py --all --output build/t-p1-2
```

## Сохранённые сценарии и новые исключения

| Проверка | Статус | Журнал |
|---|---|---|
| Host boot_info | PASS | [host-boot-info.log](host-boot-info.log) |
| Host UEFI map/EBS | PASS | [host-uefi.log](host-uefi.log) |
| Host exception decoder/formatter/reentry | PASS | [host-exception.log](host-exception.log) |
| Host memory policy/parsers | PASS | [host-memory.log](host-memory.log) |
| Host Python: диск, UART, ELF/PE proof, процесс QEMU | PASS, 39/39 | [host-python.log](host-python.log) |
| Сборки reproducibility 1 и 2 | PASS | [Первая](repro-build-one.log), [вторая](repro-build-two.log) |
| Побайтная повторяемость четырёх обычных артефактов | PASS | [Хеши](reproducibility.log) |
| Обычная сборка двух ISA | PASS | [build-for-qemu.log](build-for-qemu.log) |
| Неверный boot_info, две ISA | PASS | [build-negative.log](build-negative.log) |
| Усечённая карта, две ISA | PASS | [build-negative-memory.log](build-negative-memory.log) |
| Отдельные BRK/UD2-образы | PASS | [build-exception.log](build-exception.log) |
| x86 normal / неверный handoff / усечённая карта | PASS | [normal](qemu-x86_64-normal.log), [handoff](qemu-x86_64-invalid.log), [map](qemu-x86_64-invalid-map.log) |
| ARM normal / неверный handoff / усечённая карта | PASS | [normal](qemu-aarch64-normal.log), [handoff](qemu-aarch64-invalid.log), [map](qemu-aarch64-invalid-map.log) |
| ARM normal с начальным EL2 / с 256 МиБ | PASS | [EL2](qemu-aarch64-el2.log), [256 MiB](qemu-aarch64-256m.log) |
| Реальный x86 #UD, vector 6 | **PASS** | [qemu-x86_64-exception.log](qemu-x86_64-exception.log) |
| Реальный ARM BRK, начальный EL1 | **PASS** | [qemu-aarch64-exception.log](qemu-aarch64-exception.log) |
| Реальный ARM BRK, начальный EL2 → EL1 | **PASS** | [qemu-aarch64-exception-el2.log](qemu-aarch64-exception-el2.log) |

Сначала проходят valid handoff и диагностическая карта памяти. Затем только в отдельном тестовом образе исполняется fault. Их предшествующие `HANDOFF PASS` и `MEMMAP PASS` — результаты отдельных стадий, а не успешное завершение аварийного образа. После исключения обычного `STYLEOS KERNEL HALT` нет.

Примеры настоящих записей:

```text
STYLEOS EXCEPTION arch=x86_64 kind=sync vector=6 reason=invalid-opcode pc=0x000000000602db42 error=absent cr2=absent
STYLEOS EMERGENCY HALT

STYLEOS EXCEPTION arch=aarch64 kind=sync class=0x3c reason=brk pc=0x0000000040083908 esr=0x00000000f2000120 elr=0x0000000040083908 spsr=0x00000000600003c5 far=absent
STYLEOS EMERGENCY HALT
```

В x86-тесте до UD2 дополнительно печатается служебный `STYLEOS EXCEPTION TEST ... pc=...`; он не является записью обработчика. Полная аварийная запись с префиксом `STYLEOS EXCEPTION arch=` в каждом сценарии ровно одна, аварийный halt также один.

## Независимая сверка PC и образов

[pc-evidence.json](pc-evidence.json) связывает фактический UART с [x86 fault-site](fault-sites/x86_64.json) и [ARM fault-site](fault-sites/aarch64.json):

- x86: фактическая база Loaded Image `0x6029000` + RVA `0x4b42` = сохранённый RIP `0x602db42`. Preferred ImageBase `0x140000000` иной; подмена его фактической базой дала бы неверный результат. В исполняемой секции PE по этому RVA проверены байты `0f0b` (UD2).
- ARM: saved PC/ELR `0x40083908` совпадает с ELF-символом; ELF и реально загружаемый raw Image содержат там `002420d4` (BRK #0x120). Это подтверждено для входов EL1 и EL2.

Метаданные связываются SHA-256 с исполняемыми файлами и загружаемыми raw/GPT-образами. [artifacts.json](artifacts.json) относится к обычной сборке; [exception-artifacts.json](exception-artifacts.json) — только к тестовым аварийным образам. После изменения кода PC пересчитывается из новой сборки, а не берётся из этих примеров. Сам обработчик память по PC/FAR/CR2 не читает.

## ARM64-only и команда владельца

```sh
CC=clang python tools/test.py --host --qemu --qemu-arch aarch64 --output build/termux
```

Эта команда выполнена в **Linux**, 16/16 PASS. Для проверки независимости маршрута `OVMF_CODE`, `OVMF_VARS`, `LLD_LINK` и `QEMU_X86` были намеренно направлены на недоступные пути; точные значения сохранены в environment.json. Сборки и QEMU jobs выбирали только ARM64. Host-тесты PE-файлов являются чистым разбором тестовых данных и не требуют x86 linker/firmware.

Результат не является новым прогоном Android. Для телефона нужно получить свежие версии инструментов, `results.json` и UART-журналы из этой ветки. Прежний [T-P1.1 телефонный отчёт](../t-p1-1/termux-arm64.md) относится к предыдущим исходникам.

## Найденное и исправленное при review

Независимое [review](review.md) выявило P2 в harness: процесс мог напечатать правильные строки и самостоятельно завершиться, а тест — сообщить PASS. Исправлено: после полного маркера наблюдается живой процесс ещё 200 мс; natural exit/reset и недопустимый status отвергаются. Только затем harness останавливает QEMU. Возврат 0 после запрошенного SIGTERM допустим: QEMU обрабатывает сигнал штатно.

Восемь процессных регрессий используют изолированные **fake subprocesses**, не выдаваемые за загрузку ОС. [Их журнал](review-host/process-tests.log) и реальные QEMU-журналы разделены. Строка `[harness]` сохраняет факт намеренной остановки, natural exit, returncode и завершение окна наблюдения. `terminating on signal 15` в конце реальных логов — ожидаемая остановка VM самим тестом, не исключение гостя.

## Что сохранено и что не доказано

[tested-sources.json](tested-sources.json) фиксирует проверенные исходники и инструменты; [protected-sources.json](protected-sources.json) — 14 файлов, оставшихся побайтно идентичными принятой базе, включая boot_info, карту памяти и её парсеры, EBS, UART и linker script. Отдельно проверено отсутствие test-trigger символов в обычных объектах. [flag-safety.json](flag-safety.json) подтверждает отказ от записи exception-варианта в обычный build и несовместимость с прежним negative-флагом.

Историческое `0x96000061` классифицируется как alignment-fault в host-тесте; это **не новый аппаратный Data Abort**. BRK FAR отсутствует, #UD hardware error отсутствует. Реальные #PF/Data Abort, отказ стека/UART и полноценные IRQ/NMI/SError не проверялись и не объявляются поддержанными.

Покрытие начинается после собственных VBAR/IDT. Зависимость x86 от текущих firmware GDT/таблиц трансляции сохраняется; GDT/TSS/IST и аллокатор не вводятся. На повторном синхронном сбое guard останавливает CPU без рекурсивного UART. Восстановления/возврата к инструкции, новой модели ядра, GUI или анимации «Офис» нет. Пределы — [ADR-0005](../../adr/0005-early-exception-diagnostics.md).
