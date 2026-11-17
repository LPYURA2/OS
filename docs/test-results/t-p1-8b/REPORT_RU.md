# StyleOS T-P1.8B — внешняя программа с виртуального диска

StyleOS читает отдельно собранный `app.bin` через VirtIO, проверяет и копирует
его машинный код в статическую страницу и действительно исполняет в EL0.
В одной VM первый запуск вызывает ожидаемый аппаратный fault; после возврата
под SAFE и освобождения учёта слота второй, новый вход в EL0 завершается
через `SVC #2`. Другой payload на свежем диске возвращает другой результат
при том же boot image. Негодный executable в исправном хранилище отвергается
до копирования и EL0.

Это ограниченный AArch64 QEMU-эксперимент, не готовая ОС или общая платформа
приложений. Решение и причины: [ADR-0012](../../adr/0012-external-disk-app.md).
Учебная карта: [LEARNING_RU](../../LEARNING_RU.md), воспроизведение:
[TESTING](../../TESTING.md#внешняя-программа-с-диска-t-p18b).

## Коммиты и публикация

- Точная база: [`1ea8d090cd12c4a5d20f628c927fcba787aede98`](https://github.com/LPYURA2/OS/commit/1ea8d090cd12c4a5d20f628c927fcba787aede98), принятая отчётная вершина T-P1.8A с исправлением host-тестов Termux.
- Проверенный код: [`0ef43423de1fc4820c7efece9c3da286879d7b49`](https://github.com/LPYURA2/OS/commit/0ef43423de1fc4820c7efece9c3da286879d7b49). Первичный новый прогон закреплён на этом SHA, с чистым Git-деревом.
- Отдельный отчётный коммит содержит этот отчёт, текстовые свидетельства и закреплённый SHA в TESTING. Его SHA сообщён при сдаче; он доступен через `git log -1 --format=%H -- docs/test-results/t-p1-8b/REPORT_RU.md`.
- Публикуется только [`feature/styleos-t-p1-8b`](https://github.com/LPYURA2/OS/tree/feature/styleos-t-p1-8b), обычным push без переписывания истории.

Сверка [scope-audit.json](scope-audit.json) защищает корневой README и все
1254 исторических файла свидетельств; точная база содержит 1382 tracked files.
`main`, все прежние refs, `docs/styleos-readme` и Portable-os сохраняются.
Нет merge, force push, release или изменения чужого репозитория.

## Фактические проверки 17 ноября 2026 года

| Проверка | Результат | Свидетельство |
|---|---|---|
| Точная база, прежний полный набор | PASS, 57/57, exit 0 | [baseline/full/results.json](baseline/full/results.json); 262 Python tests, OK |
| Точная база, прежний ARM-only | PASS, 43/43, exit 0 | [baseline/arm/results.json](baseline/arm/results.json); 262 Python tests, OK |
| Точная база, storage runner T-P1.8A | PASS, 16/16, exit 0 | [baseline/storage/test-results/results.json](baseline/storage/test-results/results.json) |
| Полный прежний Linux-набор после изменений | PASS, 57/57, exit 0 | [full-regression/results.json](full-regression/results.json), [host Python](full-regression/host-python.log) |
| Прежний ARM-only без OVMF/x86 tools | PASS, 43/43, exit 0 | [arm-regression/results.json](arm-regression/results.json), [host Python](arm-regression/host-python.log) |
| Python unittest в обоих регрессионных наборах | PASS, 296 tests, OK | Входит в host-этап наборов 57/43, не прибавляется к ним |
| Прежний storage runner T-P1.8A | PASS, 16/16, exit 0 | [storage-regression/test-results/results.json](storage-regression/test-results/results.json); Python 39 tests, OK; семь VM-фаз сохранены |
| Новый однокомандный disk-app runner, чистый кодовый SHA | PASS, 16/16, exit 0 | [new-run/test-results/results.json](new-run/test-results/results.json), [run.json](new-run/run.json), [provenance](new-run/provenance.json) |
| Все прежние бинарные артефакты | PASS, 46/46 идентичны точной базе | [artifact-comparison.json](artifact-comparison.json): 44 прежних профильных файла и storage ELF/Image |
| Обычная воспроизводимость | PASS, четыре артефакта | [reproducibility.log](full-regression/reproducibility.log), две независимые сборки |
| Python 3.14 без `os.link` | PASS: 70 tests, 69 успешных и один изолированный SKIPPED hardlink-case | [python314-no-hardlink.log](python314-no-hardlink.log); остальные проверки сохранены |
| Android/Termux, новый T-P1.8B | SKIPPED | Новый прогон владельца ещё не получен; Linux не является Android PASS |
| VirtualBox, native x86-64/ARM64, физический накопитель | SKIPPED | Эти среды и новый x86 disk-app порт не испытывались |

Новый runner включает 16 этапов: пять C host-проверок, два Python-набора,
сборку boot image, независимый binary proof, две сборки внешнего приложения,
сборку упаковщика, три VM и итоговую связь замены payload с неизменным boot.
Новые Python-наборы содержат 31 test (13 site и 18 runner); старый storage
Python содержит 39. Они также входят в регрессионные 296. Счётчики этапов,
unit tests и VM обозначают разные уровни проверки и не суммируются.

Дополнительный независимый source/sanitizer аудит не нашёл блокирующего
дефекта. Loader и FDT host-тесты прошли GCC 14.2.0 с ASan/UBSan без
диагностик. Clang sanitizer был SKIPPED из-за отсутствующих packaged runtime
библиотек, после чего те же проверки выполнены GCC. Это отдельный host-аудит,
не дополнительный VM PASS. Его материалы включены в текстовые свидетельства.

Ожидаемый отказ `reject` означает PASS host-проверки только после полного
подтверждённого отказа гостевого loader и обычного halt. Сам гость не печатает
успех исполнения программы для этого сценария. После полного положительного
или отрицательного контракта harness наблюдает halt и посылает SIGTERM;
timeout, reset, silent stop и kernel exception не принимаются за результат.

Независимая итоговая [проверка](final-review.md) не обнаружила блокирующих дефектов: повторно связала финальные UART/JSON с boot/app/disk bytes, отвергла 690 пропусков обязательных строк и 31 направленную мутацию. Это отдельный аудит доказательств, не дополнительные VM-прогоны.

## Что прочитал и исполнил гость

Оба payload собраны обычными Clang/LLD из отдельного
[ASM-файла](../../../apps/disk_demo.S) и [linker script](../../../apps/disk_demo.ld).
Их нет среди linker inputs ядра. ELF служит доказательством происхождения;
в дисковый файл попадают 32 байта header и 28 байт семи AArch64-инструкций.
Bootargs выбирает только `run` или `reject`, не передаёт payload.

| Свежий диск / VM | Файл и результат | READ / WRITE / FLUSH |
|---|---|---|
| `variant-a` | `app.bin`, 60 байт; fault при X0=0, затем X0=1: X19=`0x1818a001`, exit=41 | 14 / 0 / 0 |
| `variant-b` | Отдельно собранные 60 байт; тот же fault, затем X19=`0x1818b002`, exit=73 | 14 / 0 / 0 |
| `reject` | Исправное storage generation 1, 60-байтный executable с испорченным magic; loader отказал, запусков EL0 нет | 14 / 0 / 0 |

Сырые UART сохранены как точные `.uart.log` копии; JSON содержит их SHA:
[A](new-run/test-results/variant-a.json), [A UART](new-run/test-results/variant-a.uart.log),
[B](new-run/test-results/variant-b.json), [B UART](new-run/test-results/variant-b.uart.log),
[отказ](new-run/test-results/reject.json), [его UART](new-run/test-results/reject.uart.log).
Полные команды QEMU, PID, роли, device/inode/size и SHA дисков до/после находятся
в тех же JSON. Все три VM используют один собранный boot image; его SHA-256:
`924a6e934d5b564e9e4a587e3698254185cec7cb3a00cb4071a112ce857b29b6`. SHA ELF и manifest также одинаковы между VM.

SHA-256 контейнера A:
`db7a90ae66b980e1c1c42e73a06844e6ad54b6f55df25f172dfac219fc8b7417`.
Контейнера B:
`a4a88ccded6d8c2fd3e17b14b4c535735f62b5ed449e54cc37563e97a44dfd8f`.
Это разные файлы и разные диски, а не смена строки в ядре. SHA каждого
1-МиБ диска после своей VM совпадает с SHA до неё.

Host-side упаковщик здесь разрешён явно: он заранее создаёт только новые
эксклюзивные regular files, используя неизменённый формат T-P1.8A и прежний
`kernel/storage.c`. Он не открывает пользовательские диски и не пишет во время
или после VM. Старый storage runner не изменён и по-прежнему начинает с нулевых
дисков, а содержимое сохраняет сам guest.

## Контейнер и отказ до запуска

Тестовый формат little-endian: magic `STYAPP1\0`, version 1, ISA 183, header
32 байта, code length 28, entry offset 0, reserved 0 и CRC32 всего файла с
нулём в поле CRC. Общий предел файла 1024 байта; текущий loader дополнительно
принимает только закрытую грамматику семи integer-only инструкций. Меняться
могут лишь immediates маркера и exit code. Это не общий ELF loader, публичный
ABI, `.tea` или разрешение исполнения произвольных бинарников.

До копирования проверяются длина, переполнения, выравнивание, entry, ISA,
CRC, все opcode и отсутствие пересечения source/destination/info. Отказ не
меняет destination и результат `da_info`. Проверки неверных границ и размеров,
каждого бита opcode и атомарного отказа находятся в
[test_disk_app.c](../../../tests/test_disk_app.c). В отрицательной VM
`st_mount` и `st_read` успешны, file bytes показаны, но loader отвергает magic;
нет COPY, включения нового MMU или EL0-входа, нет встроенной замены программы.

## DMA, память и реальные кадры CPU

I/O выполняется с MMU, D-cache и I-cache off. Сначала доказываются полные
RESERVED image-owned интервалы control, workspace, tables, code/stack,
entry stack и прежних VirtIO buffers, их выравнивание и исключение остальных
живых объектов. CANDIDATE/REVIEW, boot pool и heap не используются;
`GRANTED=0`. Проверяются ELF/PT_LOAD/BSS и границы обнуления startup.

После чтения закрывается I/O, выполняются барьеры и проверяется
`requests == reads == avail == used == 14`, `writes == flushes == 0`.
После повторной проверки стабильности indices нет обращений к устройству,
включая MMIO getters. Только затем валидация/копирование, `DSB; IC IALLU;
DSB; ISB`, собственные таблицы и MMU. Кэши остаются выключены. Старый драйвер
сохраняет VERSION_1/FLUSH и прежний отказ аппаратного RO feature; новый путь
read-only обеспечен отказом WRITE/FLUSH adapters и подтверждён счётчиками/хешами,
а не заявлением нового аппаратного режима.

COPY содержит все 4096 байт страницы: ровно 28 прочитанных code bytes и нулевой
остаток, проверенные независимо вместе с CRC/SHA. APP root имеет ровно две
EL0 leaves: код `0x10000000` RX и стек `0x11000000` RW/NX. SAFE имеет ноль
user leaves. Образ ядра supervisor-only/UXN, но остаётся privileged RWX;
системный W^X не заявляется. VirtIO MMIO и probe VA отсутствуют в обоих roots.
Полное конечное замыкание таблиц проверяется по raw descriptors, а не по
декларации builder.

| Аппаратное событие | Доказанные значения |
|---|---|
| Первый свежий вход | EL0t, X0=0, entry `0x10000000`, SP `0x11001000` |
| Fault внешнего `LDR` | lower-EL synchronous vector 8, PC `0x10000008`, FAR `0x12010000`, ESR `0x92000006` — read translation fault level 2 |
| Доверенный возврат | SAFE root, исходный dispatcher SP, затем очистка live context и освобождение только учёта слота |
| Второй вход после fault | Новый контекст и очищенный стек, X0=1, новый ERET в EL0; fault PC не возобновляется |
| Штатный выход payload | `SVC #2`, ESR `0x56000002`, saved ELR `0x1000001c`, X0=41/73, X19=маркер A/B |

`0x1000001c` — адрес сразу после последней SVC, сохранённый аппаратурой;
возобновления по нему нет. Полный 320-байтный frame сохраняет все X0..X30
до scratch-регистров, SP_EL0, ELR/SPSR/ESR/FAR, активный TTBR0 и entry SP.
Current-EL exception остаётся терминальной диагностикой. Parser отвергает
чужие PC/VA/root/origin, пропуск read/drain, неверные bytes/права, отсутствие
нового входа после fault, вход после неуспешного loader и одинокую PASS-строку.

## Команды и среда

Linux x86-64, Python 3.14.10, Debian Clang/LLD 19.1.7, QEMU 10.0.13,
профиль `virt/cortex-a57`, один CPU, 128 МиБ, TCG, modern VirtIO MMIO, polling.
Точные версии и compiler/linker/QEMU commands сохранены в
[provenance](new-run/provenance.json), app metadata и каждом VM JSON.
База и итоговые guest-сборки использовали один и тот же toolchain. Host C
полного набора собирался `cc` (GCC 14.2.0), ARM-only и два дисковых runner —
Clang 19.1.7. Python 3.14 выполнялся
из `/workspace/scratch/styleos-t-p1-8a-termux-compat/python-runtime/cpython-3.14.10-linux-x86_64-gnu/bin/python3.14`.

```sh
. /workspace/scratch/styleos-toolchain/env.sh
STYLEOS_PY=/workspace/scratch/styleos-t-p1-8a-termux-compat/python-runtime/cpython-3.14.10-linux-x86_64-gnu/bin/python3.14
LLVM_OBJDUMP=/workspace/scratch/styleos-toolchain/root/usr/lib/llvm-19/bin/llvm-objdump \
  "$STYLEOS_PY" tools/test.py --all --output build/t-p1-8b
OVMF_CODE=/nonexistent/tp18b/CODE OVMF_VARS=/nonexistent/tp18b/VARS \
  LLD_LINK=/nonexistent/tp18b/lld-link QEMU_X86=/nonexistent/tp18b/qemu-x86 \
  LLVM_OBJDUMP=/workspace/scratch/styleos-toolchain/root/usr/lib/llvm-19/bin/llvm-objdump \
  CC=clang "$STYLEOS_PY" tools/test.py --host --qemu --qemu-arch aarch64 --output build/arm-only-tp18b
OVMF_CODE=/nonexistent/tp18b/CODE OVMF_VARS=/nonexistent/tp18b/VARS \
  LLD_LINK=/nonexistent/tp18b/lld-link QEMU_X86=/nonexistent/tp18b/qemu-x86 \
  LLVM_OBJDUMP=/nonexistent/tp18b/llvm-objdump \
  CC=clang "$STYLEOS_PY" tools/test_storage.py --output build/storage-regression-tp18b
OVMF_CODE=/nonexistent/tp18b/CODE OVMF_VARS=/nonexistent/tp18b/VARS \
  LLD_LINK=/nonexistent/tp18b/lld-link QEMU_X86=/nonexistent/tp18b/qemu-x86 \
  LLVM_OBJDUMP=/nonexistent/tp18b/llvm-objdump \
  CC=clang "$STYLEOS_PY" tools/test_disk_app.py --output build/termux-tp18b
```

Несуществующие OVMF/x86 paths проверяют независимость ARM маршрута. Старый
полный/ARM-only task/user-process proof сохраняет свой LLVM objdump; новый
disk-app и storage runner его не требуют. Закрытая грамматика внешнего payload
проверяет все его 28 байт; полный decode всех инструкций ядра здесь не заявляется.

В Termux после checkout кодового SHA и установки `python clang lld
qemu-system-aarch64-headless` точная одна команда:

```sh
CC=clang python tools/test_disk_app.py --output build/termux-tp18b
```

Она сама собирает образы и payload, создаёт три новых тестовых диска и
уникальный run-каталог, запускает проверки и сохраняет логи/JSON. Не нужны
GUI, сеть, root или ручное создание дисков. Полный checkout и минутная сборка
payload вне дерева ядра приведены в [TESTING](../../TESTING.md#внешняя-программа-с-диска-t-p18b).
Отсутствие `os.link` проверяется отдельно: пропускается только hardlink-кейс,
остальные regular-file/inode/size/symlink/path и VM проверки сохраняются.

## Главные исходники для изучения

| Файл | Назначение |
|---|---|
| [apps/disk_demo.S](../../../apps/disk_demo.S), [disk_demo.ld](../../../apps/disk_demo.ld) | Семь инструкций внешней программы и её самостоятельная раскладка; X0 выбирает fault/exit |
| [tools/build_disk_app.py](../../../tools/build_disk_app.py) | Обычные Clang/LLD, независимая проверка ELF/raw и упаковка тестового header |
| [tools/pack_disk_app.c](../../../tools/pack_disk_app.c) | Явная host-фикстура: свежий диск прежнего store, единственный app.bin |
| [kernel/disk_app.c](../../../kernel/disk_app.c), [disk_app.h](../../../include/styleos/disk_app.h) | Границы, CRC, конечная грамматика и атомарное копирование |
| [kernel/disk_app_demo.c](../../../kernel/disk_app_demo.c) | Admission, guest mount/read, запрет записи, drain, два запуска и проверка возврата |
| [boot/aarch64/disk_app_fdt.c](../../../boot/aarch64/disk_app_fdt.c) | Ограниченный разбор DTB и выбор объявленного VirtIO MMIO |
| [arch/aarch64/disk_app.c](../../../arch/aarch64/disk_app.c), [entry.S](../../../arch/aarch64/disk_app_entry.S) | Конечные SAFE/APP roots, настоящий ERET, полный trap frame и возврат к доверенному стеку |
| [tools/disk_app_site.py](../../../tools/disk_app_site.py), [test_disk_app.py](../../../tools/test_disk_app.py) | Независимая связь ELF/Image/контейнера, raw таблиц, UART, hardware frame и disk SHA |
| [tests/test_disk_app*](../../../tests/) | Host C и Python: границы, отказ, mutation cases, parser и безопасность output-файлов |

Начните с семи инструкций ASM и сравните их offsets с таблицей событий выше.
Затем проследите `da_validate` → `da_copy` → `da_arch_prepare` → `da_arch_run`.
После этого откройте raw UART: найдите READ, DRAIN, COPY, первый `EVENT kind=fault`, SAFE,
RELEASE и второй RUN/`EVENT kind=exit`. Так видна разница между проверкой файла, сменой
адресного пространства и фактическим исполнением CPU.

## Отдельное прежнее испытание владельца и ограничения

Владелец сообщил **T-P1.8A 16/16 PASS, exit 0** на исправленном
`60c9a18fd6b3c23865d64ca5e544faf3a23c6bbf`; два скриншота подтверждают все
семь VM-фаз. Сырые UART/JSON и версия QEMU на успешных скриншотах не приложены.
QEMU 11.1.3 наблюдался в предыдущей неудачной попытке, остановившейся на
host Python, и не приписывается успешному прогону. Это отдельный прежний
результат владельца, не Linux T-P1.8B и не Android T-P1.8B PASS.

Ресурсы фиксированы и принадлежат образу. Нет общего PMM/heap, reclaim,
нового владения RAM, общего scheduler/IRQ/SMP, FP/SIMD-контекста, произвольного
ELF/FS/process ABI, пользовательских указателей, SDK, сети, GUI или `.tea`.
Один доверенный тестовый payload и корректный QEMU VirtIO не доказывают
защиту от враждебного DMA-устройства или сбоя kernel stack. Kernel faults
не восстанавливаются; завершение одного EL0-примера не является Deep Drop.
Модель ядра и публичные архитектурные контракты остаются решениями владельца.

T-P1.8B завершает только связь внешнего файла с его проверенным EL0-исполнением.
После отчёта и публикации ветки работа останавливается. Обсуждение рабочего
стола и первого x86-64 UEFI/VirtualBox-образа — отдельный следующий этап после
принятия результата; в этой задаче он не начинается.
