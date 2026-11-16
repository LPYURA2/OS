# StyleOS T-P1.6 — две кооперативные задачи ядра

**PASS:** в Linux реально выполнены переключения двух машинных контекстов на
x86-64/OVMF и AArch64 initial EL1 / EL2→EL1. Каждая задача работает на отдельном
статическом стеке, дважды уступает CPU, продолжает выполнение и нормально
возвращается в обёртку завершения. Диспетчер продолжает работу на исходном стеке
ядра. Это прототип доверенных задач ядра на одном CPU; изоляции процессов нет.

Проверки выполнены 16 ноября 2026 года. Опубликована только ветка
[`feature/styleos-t-p1-6`](https://github.com/LPYURA2/OS/tree/feature/styleos-t-p1-6).

- База: [`ea37e76abb0a1b6e75c050c43e357d8c89981537`](https://github.com/LPYURA2/OS/commit/ea37e76abb0a1b6e75c050c43e357d8c89981537), вершина T-P1.5.
- Проверенная реализация: [`d8065f31ae292eccd2558ddc8dc2cd91b78b855f`](https://github.com/LPYURA2/OS/commit/d8065f31ae292eccd2558ddc8dc2cd91b78b855f).
- Следующий отдельный коммит содержит этот отчёт, текстовые свидетельства и
  закреплённые команды в `docs/TESTING.md`. Его SHA указан в итоговом сообщении;
  он не меняет проверенные исходники.
- `main`, прежние refs, `docs/styleos-readme`, Portable-os и корневой README
  сохранены. История не переписана; merge, force push и release не выполнялись.

## Фактические результаты

| Проверка | Статус | Свидетельство и предел результата |
|---|---|---|
| Повторный прогон базы тем же toolchain | PASS, 47/47, exit 0 | [results.json](baseline/results.json), [консоль](baseline-console.log); 138 Python tests, OK |
| Полный Linux-набор T-P1.6 | PASS, 52/52, exit 0 | [results.json](full/results.json), [консоль](full-console.log): 9 host, 3 reproducibility, 10 build, 30 QEMU |
| Python unittest полного набора | PASS, 188 tests, OK | [host-python.log](full/host-python.log); входит в один host-этап, не прибавляется к 52 |
| Linux ARM64-only без x86 tools/OVMF | PASS, 39/39, exit 0 | [results.json](arm-only-linux/results.json), [консоль](arm-console.log): 9 host, 10 build, 20 QEMU; также 188 Python tests, OK |
| x86-64/OVMF, реальные task contexts | PASS | [UART](full/qemu-x86_64-tasks.log), [JSON](full/qemu-x86_64-tasks.json) |
| AArch64 initial EL1, реальные task contexts | PASS | [UART](full/qemu-aarch64-tasks.log), [JSON](full/qemu-aarch64-tasks.json) |
| AArch64 initial EL2→EL1, реальные task contexts | PASS | [UART](full/qemu-aarch64-tasks-el2.log), [JSON](full/qemu-aarch64-tasks-el2.json) |
| Четыре обычных артефакта, независимые сборки | PASS, байты совпали | [reproducibility.log](full/reproducibility.log) |
| Сравнение прежних профилей с базой | PASS, 36/36 идентичны | [old-artifacts.json](old-artifacts.json); включает обязательные 28 и ещё 8 ownership-артефактов |
| Дополнительная native x86 Microsoft-ABI проверка | PASS | [журнал](review/msabi-host.log), [host-код](review/msabi-host.c), [ASM-адаптация для ELF](review/context-msabi-host.S); отдельна от QEMU |
| Host lifecycle под ASan/UBSan | PASS | [host-tasks-sanitized.log](review/host-tasks-sanitized.log); не доказывает машинное переключение |
| T-P1.6 Android/Termux | SKIPPED | Новый прогон владельца ещё не получен; Linux ARM-only не является Android |
| T-P1.6 VirtualBox | SKIPPED | Не запускался в этой среде |
| T-P1.6 физическое оборудование | SKIPPED | Устройство для испытаний отсутствует |

FAIL в финальных наборах нет. Прежние normal, negative handoff/map, exception,
bootpool, paging/fault и ownership/refusal сценарии сохранены; их журналы
находятся в [full/](full/) и [arm-only-linux/](arm-only-linux/). Во всех 36 старых
артефактах отсутствует маркер TASKS; их равенство базе также проверяет исключение
новых стеков и кода из старых профилей. Единственное изменение старого
ownership-теста — счётчик сборок `9 → 10` после добавления нового профиля.

## Что именно подтверждают новые запуски

Во всех трёх полных прогонах проверен один точный порядок:

```text
A:START → B:START → A:RESUME → B:RESUME → A:EXIT → B:EXIT
```

Это шесть операций GO, по две уступки на задачу и один нормальный возврат каждой
функции. После возврата стартовая обёртка переводит слот в DONE и переключается
в диспетчер. DONE больше не выбирается. Диспетчер получает EMPTY с
`completed=2`, `dispatches=6`, освобождает два слота до FREE, повторно проверяет
EMPTY, canary и архитектурное состояние. Затем следуют единственные
`STYLEOS TASKS PASS` и `STYLEOS KERNEL HALT`.

Ниже адреса именно из финальных UART, полуинтервалы стеков включают canary.
SP/PC относятся к первой сохранённой точке уступки. В PE используются реальные
адреса после загрузки/релокации, а не только RVA файла.

| Прогон / задача | Полный стек [start, end) | Адрес volatile local | Сохранённый SP | PC продолжения probe |
|---|---|---|---|---|
| `qemu-x86_64-tasks` / A | `0x0000000006026000–0x000000000602a000` | `0x0000000006029f70` | `0x0000000006029e70` | `0x0000000006014532` |
| `qemu-x86_64-tasks` / B | `0x000000000602a000–0x000000000602e000` | `0x000000000602df70` | `0x000000000602de70` | `0x0000000006014532` |
| `qemu-aarch64-tasks` / A | `0x0000000040097000–0x000000004009b000` | `0x000000004009afb8` | `0x000000004009aee0` | `0x000000004008651c` |
| `qemu-aarch64-tasks` / B | `0x000000004009b000–0x000000004009f000` | `0x000000004009efb8` | `0x000000004009eee0` | `0x000000004008651c` |
| `qemu-aarch64-tasks-el2` / A | `0x0000000040097000–0x000000004009b000` | `0x000000004009afb8` | `0x000000004009aee0` | `0x000000004008651c` |
| `qemu-aarch64-tasks-el2` / B | `0x000000004009b000–0x000000004009f000` | `0x000000004009efb8` | `0x000000004009eee0` | `0x000000004008651c` |

Локальные значения A=`0xa11ce55a12345678`, B=`0xb0b55aa987654321` остаются на
своих адресах при START/RESUME/EXIT. Диспетчер независимо сохраняет локальное
`0xd15ca11c12345678` на исходном стеке, свой SP и точный PC продолжения.
Полные адреса его контекста, начальных кадров, DONE, всех живых объектов и карты
памяти сохранены в `tasks_trace` каждого JSON.

Перед уступкой ASM записывает разные шаблоны в настоящие nonvolatile GPR:
`0x1122000000000000 + (index << 32) + tag`. Теги A — `0x101/0x102`,
B — `0x201/0x202`. После реального возобновления сравниваются живые регистры,
отдельно проверяются сырые сохранённые значения. Полная маска x86 — `0xff`
(RBX, RBP, RDI, RSI, R12–R15), ARM — `0x7ff` (X19–X29); X30/LR на ARM отдельно
связан с реальным BL-продолжением. Неиспользуемые x86-поля контекста равны нулю.

Harness проверяет весь журнал, SHA образа/manifest, диапазоны, действительные
PC, локальные значения, шаблоны, состояния, canary и завершающий halt.
Для всех пяти новых task jobs полного и ARM-only наборов JSON фиксирует
`requested_stop=true`, `forced_stop=false`, `natural_exit=false`,
`settled=true`, `returncode=0`: после терминального маркера эмулятор наблюдался
ещё 200 мс, затем штатно остановлен harness. Timeout/reset/тишина либо одна
строка PASS не засчитываются. Парсер имеет отрицательные тесты на пропуски,
повторы, неверные SP/PC/значения, повторное выполнение после EXIT и PASS-only.

## ABI, память и привязка к бинарнику

Два стека по 16384 байта и control workspace 4096 байт находятся в image-owned
BSS и выровнены на 4096. До первой записи проверяются целые диапазоны,
переполнение, RESERVED/image containment, отсутствие CANDIDATE и перекрытия с
исходным стеком, handoff, источником карты/DTB, diagnostic workspace,
boot pool/metadata и архитектурными объектами. Полное покрытие доказывается
по PE sections и фактической базе либо ELF/PT_LOAD/BSS и raw Image.
Boot pool присутствует, но в этом профиле не допускается и не используется.
Передачи общей RAM нет; смысл `GRANTED=0` сохранён.

x86 использует Microsoft x64 call boundary: switch сохраняет `RSP+8` и
PC из `[RSP]`, затем восстанавливает целевой SP и переходит на PC. Начальный
SP равен usable-top минус 40: синтетический адрес возврата в trap, 32 байта
shadow space, `SP % 16 = 8` при входе в C-функцию. SP сохранённых продолжений
выровнен на 16. Probe/dispatcher/finish/trap имеют свои корректные кадры;
red zone отключён. ARM сохраняет X19–X30, SP и BL-продолжение, восстанавливает
LR отдельно от PC; все SP выровнены на 16.

Это ограниченный **integer-only** ABI. x86 сохраняет унаследованные CR3/GDT/IDT
и IF/DF=0, ARM — EL1h/MMU-off/VBAR/DAIF. Арифметические flags и volatile GPR
не входят в сохраняемый call-boundary контракт. Общая FP/SIMD-смена контекста
не реализована. Фактические compiler flags и полный список источников
сохранены в [task manifest](sites/tasks/manifest.json) и `task-compiler.json`.

Новый профиль требует LLVM objdump. Проверка декодирует действительные байты
всех исполняемых секций связанного образа, сверяет покрытие и границы
инструкций, запрещает gaps/bad decode/FP/SIMD и несовместимые compiler flags.
Три PC продолжений находятся сразу после прямого CALL/BL к настоящему switch.
x86: 32166 байт, 8340 инструкций; ARM: 32012 байт, 7535 инструкций.
В ARM исключены только проверенные 60 байт данных Linux Image header;
1872 байта нулевого vector padding учитываются явно. Доказательства:
[x86 tasks-site](sites/tasks/x86_64/tasks-site.json),
[ARM tasks-site](sites/tasks/aarch64/tasks-site.json),
[независимая проверка](review/final-review.md).
Старые профили новую зависимость не используют.

16-байтовые программные canary с двух сторон стека сохранили значения
`0x6b6b6b6b6b6b6b6b` снизу и `0xa5a5a5a5a5a5a5a5` сверху. Они обнаруживают
некоторые повреждения после переключений, но не запрещают задаче писать в
память ядра. Аппаратных guard pages, защиты задач друг от друга и восстановления
после падения привилегированной функции нет.

## Как читать исходники

| Файл | Назначение |
|---|---|
| [include/styleos/tasks.h](../../../include/styleos/tasks.h) | Два слота, состояния, размер контекста, ограничения стеков и интерфейсы |
| [kernel/tasks.c](../../../kernel/tasks.c) | Общий C-движок учёта FREE/READY/RUNNING/DONE, выбор READY, отказы без частичной мутации |
| [kernel/tasks_demo.c](../../../kernel/tasks_demo.c) | Допуск памяти, кадры запуска, две функции задач, диспетчер, диагностика и доказательства |
| [arch/x86_64/tasks_context.S](../../../arch/x86_64/tasks_context.S) | Настоящее сохранение/восстановление x86 GPR/SP/PC, probe и обёртки |
| [arch/aarch64/tasks_context.S](../../../arch/aarch64/tasks_context.S) | Настоящее сохранение/восстановление ARM GPR/SP/LR/PC |
| [arch/x86_64/tasks.c](../../../arch/x86_64/tasks.c), [arch/aarch64/tasks.c](../../../arch/aarch64/tasks.c) | Read-only проверка текущего аппаратного режима и допустимости image-owned памяти |
| [tools/tasks_site.py](../../../tools/tasks_site.py), [tools/integer_code.py](../../../tools/integer_code.py) | Независимое от UART извлечение диапазонов/PC и полный integer-only decode |
| [tools/build.py](../../../tools/build.py), [tools/test.py](../../../tools/test.py) | Изолированный `--test-tasks`, manifests, запуск QEMU и строгая сверка фактов |
| [tests/test_tasks.c](../../../tests/test_tasks.c), [test_tasks_output.py](../../../tests/test_tasks_output.py), [test_tasks_site.py](../../../tests/test_tasks_site.py), [test_integer_code.py](../../../tests/test_integer_code.py) | Host-переходы/отказы и отрицательные проверки журнала/бинарного доказательства |

Архитектурное решение и подробные кадры: [ADR-0009](../../adr/0009-cooperative-kernel-tasks.md).
Учебный разбор: [LEARNING_RU](../../LEARNING_RU.md). Общая точка `kernel_main()`
вызывает demo только при `STYLEOS_TEST_TASKS`; обычный путь сохранён.
Все девять тестовых флагов несовместимы попарно; новый выход — `build/tasks`
или подкаталог `tasks` внутри выбранного runner output.

## Воспроизведение

Версии: Clang/LLD/LLVM objdump 19.1.7, host GCC 14.2.0, Python 3.12.14,
QEMU 10.0.13, Linux x86-64. Точные пути, firmware SHA, команды и exit codes:
[environment.json](environment.json). Toolchain и firmware находятся вне Git.
LLVM objdump дополнительно извлечён в пользовательский toolchain из Debian
`llvm-19_19.1.7-3+b1_amd64.deb` после проверки SHA256; пакет не публикуется.

Полный прогон после настройки Clang/LLD/QEMU/OVMF:

```sh
python3 tools/test.py --all --output build/t-p1-6
```

Проверенная независимость Linux ARM-only от отсутствующих x86 средств:

```sh
OVMF_CODE=/nonexistent/styleos-t-p1-6/OVMF_CODE \
OVMF_VARS=/nonexistent/styleos-t-p1-6/OVMF_VARS \
LLD_LINK=/nonexistent/styleos-t-p1-6/LLD_LINK \
QEMU_X86=/nonexistent/styleos-t-p1-6/QEMU_X86 \
CC=clang python3 tools/test.py --host --qemu --qemu-arch aarch64 --output build/termux-tp16
```

В фактической среде сначала загружен внешний `env.sh`, а `LLVM_OBJDUMP` явно
указывал на LLVM 19; точные команды зафиксированы в environment.json.

Для **нового каталога Termux**:

```sh
pkg install git python clang lld llvm qemu-system-aarch64-headless
git clone --branch feature/styleos-t-p1-6 --single-branch https://github.com/LPYURA2/OS.git styleos-tp16
cd styleos-tp16
git checkout --detach d8065f31ae292eccd2558ddc8dc2cd91b78b855f
git rev-parse HEAD
python --version
clang --version
ld.lld --version
llvm-objdump --version
qemu-system-aarch64 --version
CC=clang python tools/test.py --host --qemu --qemu-arch aarch64 --output build/termux-tp16
echo $?
cat build/termux-tp16/test-results/results.json
```

Если имя decoder отличается, задайте `LLVM_OBJDUMP=/путь/к/llvm-objdump`.
Runner сам останавливает QEMU. Сохраните все `test-results/*.log` и `*.json`,
особенно `host-tasks.log`, `host-python.log`, `build-tasks.log`,
`qemu-aarch64-tasks.log/.json`, `qemu-aarch64-tasks-el2.log/.json`, прежние
ownership/paging/pool/exception журналы; также `tasks/manifest.json`,
`tasks/aarch64/tasks-site.json`, `task-compiler.json`, прежние manifests/sites,
SHA checkout и версии инструментов. Этот маршрут не запускает x86 или
reproducibility. Его будущий результат надо учитывать отдельно от Linux.

## Историческое дополнение о телефоне

В задании T-P1.6 владелец сообщил независимый Android/Termux-прогон **T-P1.5**:
**35/35, exit 0, QEMU 11.1.3, по скриншоту**. Это owner-reported подтверждение
предыдущего этапа. Сам скриншот, UART и JSON в текущем задании отсутствуют,
поэтому повторной технической проверки этих файлов здесь нет. Исторические
отчёты и их checksums не изменены. Эти 35/35 не входят в счётчики T-P1.6 и не
снимают SKIPPED для нового телефонного прогона.

## Целостность, публикация и дальнейшая граница

[tested-sources.json](tested-sources.json) связывает SHA256 всех 84 tracked
implementation/build/test файлов с коммитом реализации.
[final-review.json](review/final-review.json) дополнительно содержит снимок 22
изменённых/новых файлов реализации, все 36 старых артефактов и хеши фактических
task UART/JSON/sites/manifest. Независимая проверка повторно декодировала
финальные PE/ELF/Image и восстановила сырые UART из логов. Все 490 исторических
файлов свидетельств и байты корневого README совпали с базой.

Контрольные суммы текстовых свидетельств этого каталога:

```sh
cd docs/test-results/t-p1-6
sha256sum -c SHA256SUMS
```

Снимки [OS до](review/os-refs-before.txt) / [до публикации](review/os-refs-final.txt)
и [Portable-os до](review/portable-refs-before.txt) /
[после проверки](review/portable_os-refs-final.txt) идентичны.
Запись разрешена только в новую task branch; после push отдельно сверяются
удалённый SHA/tree и неизменность всех остальных refs.

Общий PMM/heap, использование CANDIDATE/REVIEW, reclaim, новая GDT/MMU-политика,
IRQ/таймер/вытеснение/SMP, пользовательский режим, syscalls/IPC, драйверы, GUI,
Deep Drop, общая FP/SIMD-смена контекста и промышленная модель ядра не
реализованы этим этапом. Доверенная задача с ошибкой может повредить ядро.
T-P1.7 должен отдельно проверить сдерживание сбоя пользовательского процесса;
T-P1.6 этого не доказывает и не начинает. После отчёта и публикации работа
останавливается до нового решения владельца.
