# StyleOS T-P1.7 — изоляция одного сбоя пользовательского процесса

**PASS:** процессор действительно выполнил две встроенные пользовательские
программы в CPL3 на x86-64 и EL0 на AArch64. A после уступки и возобновления
попытался прочитать отсутствующий в его адресном пространстве VA приватной страницы B.
Аппаратный #PF / Data Abort завершил только A. После этого B продолжил работу
на своём сохранённом контексте, проверил прежние данные, изменил их и штатно
вышел. Ядро вернулось в диспетчер, выполнило финальные проверки и штатный halt.

Это ограниченный эксперимент с двумя известными integer-only программами,
одним CPU и конечными image-owned ресурсами. Полная безопасность произвольных
программ, восстановление после ошибки ядра и промышленная ABI не заявляются.

## Коммиты и область публикации

- База: [`eb73fad3ed431e91ebe995be4b370c678210e2b8`](https://github.com/LPYURA2/OS/commit/eb73fad3ed431e91ebe995be4b370c678210e2b8), опубликованный отчёт T-P1.6.
- Проверенная реализация: [`af898029ff71892d8e3c6a3308263624af722130`](https://github.com/LPYURA2/OS/commit/af898029ff71892d8e3c6a3308263624af722130).
- Следующий отдельный коммит содержит этот отчёт, текстовые свидетельства и
  закреплённый SHA в TESTING. Его SHA передан владельцу в итоговом сообщении;
  он не меняет проверенный код. Локально: `git log -1 --format=%H -- docs/test-results/t-p1-7/README.md`.
- Единственная публикуемая ветка — [`feature/styleos-t-p1-7`](https://github.com/LPYURA2/OS/tree/feature/styleos-t-p1-7).
  `main`, прежние refs, `docs/styleos-readme`, корневой README, Portable-os и
  история сохранены. Merge, force push и release не выполнялись.

## Фактические проверки 16 ноября 2026 года

| Проверка | Статус | Свидетельство и граница |
|---|---|---|
| Повторный полный прогон точной базы | PASS, 52/52, exit 0 | [results.json](baseline/results.json), [консоль](baseline-console.log); 188 Python tests, OK |
| Полный Linux-набор новой реализации | PASS, 57/57, exit 0 | [results.json](full/results.json), [консоль](full-console.log): 10 host, 3 reproducibility, 11 build, 33 QEMU |
| Python unittest полного набора | PASS, 219 tests, OK | [host-python.log](full/host-python.log); один host-этап внутри 57, не добавляется к счётчику этапов |
| Linux ARM64-only с отсутствующими OVMF/x86 tools | PASS, 43/43, exit 0 | [results.json](arm-only-linux/results.json), [консоль](arm-console.log): 10 host, 11 build, 22 ARM QEMU; также 219 Python tests, OK |
| x86-64/OVMF: CPL3, #PF A, B после fault | PASS | [UART](full/qemu-x86_64-user-processes.log), [JSON](full/qemu-x86_64-user-processes.json) |
| ARM64 initial EL1: EL0, Data Abort A, B после fault | PASS | [UART](full/qemu-aarch64-user-processes.log), [JSON](full/qemu-aarch64-user-processes.json) |
| ARM64 initial EL2→EL1: тот же пользовательский сценарий | PASS | [UART](full/qemu-aarch64-user-processes-el2.log), [JSON](full/qemu-aarch64-user-processes-el2.json) |
| Старые артефакты, включая T-P1.6 | PASS, 40/40 байтовых совпадений | [old-artifacts.json](old-artifacts.json), тот же toolchain; 10 прежних профилей × четыре артефакта |
| Воспроизводимость обычных образов | PASS, четыре совпадения | [reproducibility.log](full/reproducibility.log), две независимые сборки |
| Host lifecycle под ASan/UBSan | PASS, exit 0 | [журнал](review/host-user-processes-sanitized.log); не доказывает CPU privilege transition |
| Новый T-P1.7 Android/Termux | SKIPPED | Прогон владельца нового SHA ещё не получен; Linux ARM-only не является Android |
| Новый T-P1.7 VirtualBox | SKIPPED | Не запускался в этой среде |
| Новый T-P1.7 на физическом оборудовании | SKIPPED | Устройство для испытаний отсутствует |

FAIL в финальных наборах нет. Сохранены normal, negative handoff/map, ранние
исключения, bootpool, paging/fault, ownership/refusal и кооперативные kernel
tasks. Во всех 40 старых артефактах отсутствует новый маркер `STYLEOS USER`;
точное равенство базе дополнительно проверяет исключение новых ресурсов и
кода из старых профилей. Исторические тесты inventory обновлены для нового
host/build/QEMU этапа с сохранением проверок прежних подмножеств.

## Аппаратный сценарий и доказательство продолжения B

Во всех новых прогонах есть ровно четыре GO и четыре аппаратных входа:

```text
A: YIELD → B: YIELD → A: FAULT → B: EXIT
```

Первый вход A и B фиксирует настоящий сохранённый пользовательский PC/SP и
привилегированный стек входа. B записывает `0x123456789abcdef0` в свою приватную
страницу и локальную ячейку своего стека. После fault A ядро возобновляет B с
ранее сохранённого PC/SP. Пользовательский код B проверяет эти значения и
сохранённые регистры, пишет `0xfedcba9876543210` и делает ограниченный EXIT.
Следовательно, финальная запись B выполнена именно после исключения A.

| Реальный прогон | Fault PC A | Аппаратная причина | PC возобновления B | Saved PC штатного EXIT B |
|---|---|---|---|---|
| x86-64/OVMF | `0x1000002c`, `MOV (%RDI), RAX`, байты `488b07` | vector 14, error `0x4`: P=0, W/R=0, U/S=1 | `0x10000039` | `0x10000069`, после INT 0x80 |
| ARM initial EL1 | `0x10000040`, `LDR X1,[X0]`, байты `010040f9` | ESR `0x92000007`: Data Abort lower EL, read, L3 translation fault, FAR valid | `0x1000003c` | `0x100000a0`, после SVC #2 |
| ARM initial EL2→EL1 | `0x10000040` | Та же проверенная причина и отдельный журнал | `0x1000003c` | `0x100000a0` |

Во всех случаях CR2/FAR равен `0x12010000`: VA данных B. Эта страница
отображена под корнем B и отсутствует под корнем A. На x86 saved CS=`0x23`,
SS=`0x1b`, CPL3; на ARM SPSR содержит EL0t с маскированными DAIF и допустимыми
NZCV, вход идёт через lower-AArch64 vector slot 8. Kernel-origin syndrome,
неверный fault PC/VA, другой процесс или иной тип исключения отвергаются.

Фактически наблюдаемые корни и полный разбор их таблиц:

| Прогон | SAFE root / использовано страниц | A root / страниц | B root / страниц |
|---|---|---|---|
| x86-64/OVMF | `0x05fbf000` / 5 | `0x05fdf000` / 8 | `0x05fff000` / 8 |
| ARM initial EL1 | `0x4009f000` / 6 | `0x400bf000` / 9 | `0x400df000` / 9 |
| ARM initial EL2→EL1 | `0x4009f000` / 6 | `0x400bf000` / 9 | `0x400df000` / 9 |

SAFE имеет ноль пользовательских leaves, A и B — ровно по три. Для каждого
корня guest сканирует все 32 страницы резерва и печатает каждый ненулевой
дескриптор. Независимый host-walker восстанавливает полное дерево, проверяет
его замыкание внутри своего резерва и точное равенство ожидаемому набору
leaves. Дополнительные пользовательские алиасы, чужие приватные PA, доступ
к ядру и RWX user leaves не проходят проверку.

В обоих ISA пользовательский SP A=`0x11000ff0`, B=`0x11010ff0` находится
в собственном стеке. Привилегированный raw-frame SP — `0x06022f50` на x86 и
`0x40102ec0` на ARM; целиком 176/320 байт кадра лежат в отдельном entry stack.
Исходный dispatcher SP и volatile cookie `0xd15ca11c12345678` сохраняются.
Подробности адресов, регистров, таблиц, данных и стадий —
[scenario-summary.json](scenario-summary.json) и `user_processes_trace` каждого JSON.

После каждого аппаратного входа фактический CR3/TTBR0 возвращается на SAFE,
затем восстанавливается исходный стек диспетчера и сбрасываются активные
указатели контекста. Только после этого принимается событие состояния.
A становится FAULTED, больше не выбирается и не возвращается к fault PC.
После EXIT B проверяются EMPTY, `faulted=1`, `exited=1`, `dispatches=4`.
Оба слота очищаются до FREE под SAFE. Физические страницы и неактивные таблицы
остаются в резерве образа. Финальная проверка ядра предшествует единственным
`STYLEOS USER PASS` и `STYLEOS KERNEL HALT`.

Во всех пяти новых user-processes запусках полного и ARM-only наборов JSON фиксирует
`requested_stop=true`, `forced_stop=false`, `natural_exit=false`,
`settled=true`, `returncode=0`. Harness наблюдает эмулятор ещё 200 мс после
терминального маркера, затем намеренно останавливает его. Timeout/reset,
тишина, естественное завершение QEMU и одна строка PASS не засчитываются.

## Память, права и контексты

Новый профиль `--test-user-processes` использует отдельный выход
`build/user-processes` или подкаталог `user-processes` runner output.
Все десять тестовых флагов попарно несовместимы.

| Image-owned объект | Полный размер и назначение |
|---|---|
| `styleos_user_tables` | 3 × 32 × 4096 = 393216 байт; конечные резервы SAFE/A/B |
| `styleos_user_control` | 16384 байта; состояния, пользовательские contexts, snapshots и последние 4096 байт arch scratch |
| `styleos_user_entry_stack` | 16384 байта; аппаратный вход в ядро независимо от пользовательского SP |
| `styleos_user_code_a/b` | По 4096 file-backed байт, отдельные встроенные ASM-программы с NOP padding |
| `styleos_user_stack_a/b`, `styleos_user_data_a/b` | По 4096 BSS-байт на каждый объект; собственные стек и данные каждого процесса |
| x86 GDT / TSS / IDT | Три отдельные страницы по 4096 байт |
| ARM user vectors | Отдельные 2048 байт, выравнивание 2048 |
| Прежний стек ядра и прочие живые объекты | Сохраняются с прежними владельцами; диспетчер работает на исходном 65536-байтном стеке |

До новых записей проверяются целые полуинтервалы, alignment, переполнение,
RESERVED/image containment и непересечение с живыми объектами. x86 сначала
выполняет ограниченный read-only proof унаследованного identity mapping, ARM —
проверку фиксированного Image и MMU-off входа. Независимое binary evidence
покрывает полные PE sections/runtime relocation и ARM ELF/PT_LOAD/BSS/raw
Image. Приведение произвольного указателя к PA не используется как контракт.
Нет выделения из boot pool, CANDIDATE/REVIEW, общего PMM/heap или reclaim;
смысл `GRANTED=0` сохранён. `boot_info v1` и нормализация памяти не менялись.

В каждом user root код отображён по VA `0x10000000` как RX, собственный стек
по A=`0x11000000` / B=`0x11010000` и данные по A=`0x12000000` /
B=`0x12010000` — RW/NX. Физические code/stack/data страницы разные.
Весь образ ядра остаётся supervisor-only; его identity-алиасы не дают user
доступа к собственным или чужим физическим страницам через kernel VA.
На ARM отдельный PL011 — привилегированный device/NX mapping.
Supervisor image closure остаётся RWX: системный W^X не реализован.

x86 использует собственные CS=`0x08`, SS=`0x10`, user CS=`0x23`, SS=`0x1b`,
TSS selector=`0x28`, TSS.RSP0 на entry stack и DPL3 interrupt gate INT 0x80.
Переход GDT сохраняет временный совместимый alias старого CS для старой IDT,
затем после установки новой IDT сужает GDT limit до 55. ARM использует
собственные stage-1 таблицы и VBAR, SP_EL1 для lower-EL входа, SVC #1/#2.
Минимальные YIELD/EXIT не принимают пользовательских указателей. Снимки данных
и stack-cookie читаются только через доказанные алиасы известных приватных страниц.

Кадр исключения сохраняет все 15 non-SP GPR x86 с RIP/RSP/RFLAGS/CS/SS либо
все X0–X30 со SP_EL0/ELR/SPSR на ARM. Контекст T-P1.6 для C-вызова здесь не
подменяет аппаратный кадр. Перед YIELD B сохраняет также volatile R10/X9
`0x55aa001122334455`, проверяет его после возврата вместе с nonvolatile GPR,
stack-cookie и приватными данными. FP/SIMD и прочее полное CPU state не
сохраняются; LLVM objdump проверяет integer-only байты всего связанного образа.
Результаты: x86 51126 байт / 20322 инструкции, ARM 50912 байт / 12260 инструкций.
Допущен точный LLVM mnemonic `lretq` для существующей integer far-return
инструкции; запреты FP/SIMD сохранены.

## Файлы для изучения

| Файл | Чем управляет |
|---|---|
| [include/styleos/user_processes.h](../../../include/styleos/user_processes.h) | Размеры ресурсов, 288-байтный общий integer context, события, состояния и узкий интерфейс adapters |
| [kernel/user_processes.c](../../../kernel/user_processes.c) | FREE/READY/RUNNING/FAULTED/EXITED, четыре разрешённых события, отказ без частичной мутации, release только под SAFE |
| [kernel/user_processes_demo.c](../../../kernel/user_processes_demo.c) | Допуск памяти, запуск A/B, проверка аппаратных кадров, данных/стеков, таблиц, диспетчера и финального результата |
| [arch/x86_64/user_processes.c](../../../arch/x86_64/user_processes.c), [ASM entry](../../../arch/x86_64/user_processes_entry.S) | Старый read-only proof, новые roots/GDT/TSS/IDT, IRETQ в CPL3, INT/#PF capture и безопасный возврат; две tiny user-программы |
| [arch/aarch64/user_processes.c](../../../arch/aarch64/user_processes.c), [ASM entry](../../../arch/aarch64/user_processes_entry.S) | MMU/AP/UXN/PXN, новые vectors, ERET в EL0, SVC/Data Abort, полный capture/restore и программы |
| [tools/user_processes_site.py](../../../tools/user_processes_site.py) | PE/ELF/Image extents, code-page SHA, точные user PC/opcode и integer-only proof |
| [tools/build.py](../../../tools/build.py), [tools/test.py](../../../tools/test.py) | Изолированная сборка, manifests, настоящий QEMU и независимый разбор всего дерева таблиц/журнала |
| [tests/test_user_processes.c](../../../tests/test_user_processes.c), [output](../../../tests/test_user_processes_output.py), [site](../../../tests/test_user_processes_site.py), [manifest](../../../tests/test_user_processes_manifest.py) | Переходы/отказы и отрицательные проверки происхождения, PC/VA, порядка, полноты кадров, алиасов и подмены metadata/образов |

Подробный архитектурный контракт — [ADR-0010](../../adr/0010-user-process-fault-isolation.md),
учебное объяснение — [LEARNING_RU](../../LEARNING_RU.md).

## Воспроизведение и новый Termux-прогон

Проверенная среда: Linux x86-64, Clang/LLD/LLVM objdump 19.1.7, GCC 14.2.0,
Python 3.12.14, QEMU 10.0.13, внешний OVMF. Точные версии, firmware SHA,
пути, команды и exit codes — [environment.json](environment.json). Toolchain
и firmware не входят в Git. Полная команда после их настройки:

```sh
python3 tools/test.py --all --output build/t-p1-7
```

Фактически проверенный Linux ARM-only маршрут намеренно получил отсутствующие
OVMF_CODE, OVMF_VARS, LLD_LINK и QEMU_X86. Точная команда в environment.json;
выбор ARM не обращается к этим средствам. LLVM objdump нужен на обоих ISA.

Для отдельного нового каталога **Android/Termux**:

```sh
pkg install git python clang lld llvm qemu-system-aarch64-headless
git clone --branch feature/styleos-t-p1-7 --single-branch https://github.com/LPYURA2/OS.git styleos-tp17
cd styleos-tp17
git checkout --detach af898029ff71892d8e3c6a3308263624af722130
git rev-parse HEAD
python --version
clang --version
ld.lld --version
llvm-objdump --version
qemu-system-aarch64 --version
CC=clang python tools/test.py --host --qemu --qemu-arch aarch64 --output build/termux-tp17
echo $?
cat build/termux-tp17/test-results/results.json
```

При другом имени decoder задайте `LLVM_OBJDUMP=/путь/к/llvm-objdump`.
Harness сам завершает QEMU. Сохраните все `test-results/*.log` и `*.json`,
особенно `host-user-processes.log`, `host-python.log`, `build-user-processes.log`,
`qemu-aarch64-user-processes.log/.json`, `qemu-aarch64-user-processes-el2.log/.json`,
также старые task/paging/ownership/exception/pool журналы. Нужны
`user-processes/manifest.json`, `user-processes/aarch64/user-processes-site.json`,
`user-processes-compiler.json`, старые manifests/sites, SHA checkout и версии.
Этот маршрут не запускает x86 или reproducibility. До нового фактического
прогона Android T-P1.7 остаётся SKIPPED.

## Отдельное дополнение о T-P1.6 на телефоне

Источник — задание владельца T-P1.7: **T-P1.6 Android/Termux — 39/39 PASS,
код 0**, SHA `d8065f31ae292eccd2558ddc8dc2cd91b78b855f`, подтверждение по
скриншотам, оба ARM task-сценария PASS. Предварительная проверка
`llvm-objdump` показала **21.1.8**. Версия QEMU в этих скриншотах не показана;
сырые ARM UART/JSON не приложены. Здесь это owner-reported результат прошлого
этапа, без повторного анализа отсутствующих файлов. Исторические отчёты и
checksums сохранены; это не новый телефонный PASS T-P1.7.

## Целостность, проверенная граница и остановка

[tested-sources.json](tested-sources.json) связывает SHA256 всех 96 tracked
implementation/build/test файлов с коммитом реализации. [scope-audit.json](scope-audit.json)
фиксирует 781 базовый файл, 681 неизменённый исторический файл свидетельств и
неизменность корневого README. Отдельные [x86](review/x86-audit.md) и
[ARM](review/arm-audit.md) source audits проверяют privilege/trap/mapping path;
финальная [независимая проверка](review/final-proof-audit.md) повторно связывает
реальные linked images, manifest, raw UART и JSON. Текстовые свидетельства:

```sh
cd docs/test-results/t-p1-7
sha256sum -c SHA256SUMS
```

Перед публикацией повторно сверяются OS/Portable-os refs; после отправки —
итоговый SHA/tree на GitHub и неизменность остальных refs. Снимки находятся
в [review/](review/). Запись разрешена только в новую task branch.

Проверено сдерживание одного выбранного user-read translation fault A и
реальное продолжение B после него. Не проверены произвольные программы и
все возможные нарушения прав. Supervisor image RWX, ограниченные ресурсы и
test-only операции остаются пределами эксперимента. Нет общего PMM/heap,
reclaim, общего FP/SIMD-контекста, IRQ/таймера/вытеснения/SMP, production process
ABI, приложений/GUI, автоматического рестарта служб, Deep Drop или восстановления
ядра после сбоя. Неожиданное исключение ядра остаётся диагностическим terminal
failure и не засчитывается как завершение A. Повреждение privileged stack,
TSS/vector path или UART не покрывается обещанием recovery.

Работа останавливается после отчёта и публикации. T-P1.8 не начат;
дальнейшие инженерные решения принимает владелец.
