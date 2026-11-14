# T-P1.5 — read-only план защиты физической памяти

База: [`8bc7e78592c1aef78429042658f9ae02f71637c5`](https://github.com/LPYURA2/OS/commit/8bc7e78592c1aef78429042658f9ae02f71637c5). Проверенный код: [`a284c30abbea2999eeaef721e0bd380c582c6dd0`](https://github.com/LPYURA2/OS/commit/a284c30abbea2999eeaef721e0bd380c582c6dd0). Ветка: [`feature/styleos-t-p1-5`](https://github.com/LPYURA2/OS/tree/feature/styleos-t-p1-5). Отдельный отчётный коммит добавляет документацию, свидетельства и закреплённые команды без изменения проверенных исходников.

План структурно проверяет заявленный набор защит, но **не передаёт ни одной страницы**. `GRANTED=0` во всех положительных результатах. Общий PMM, reclaim и следующий этап не реализованы.

## Фактические результаты

| Проверка | Статус | Свидетельство |
|---|---|---|
| База T-P1.4 до изменений, новый полный запуск | PASS, 37/37 | [JSON](baseline/results.json), [console](baseline/console.log) |
| Полный Linux T-P1.5 | **PASS, 47/47**, exit 0 | [results.json](results.json), [console](full-console.log) |
| Host Python внутри полного набора | **PASS, 138/138 unittest** | [host-python.log](host-python.log) |
| Linux ARM64-only без OVMF/x86 linker/QEMU | **PASS, 35/35**, exit 0 | [JSON](arm-only/results.json), [console](arm-only/console.log) |
| Все 28 артефактов семи старых профилей против базы | PASS, побайтно одинаковы | [Независимая сверка](review/final-artifact-review.json) |
| Четыре обычных артефакта в двух независимых сборках | PASS | [reproducibility.log](reproducibility.log) |
| Новый код/буферы отсутствуют в старых профилях | PASS | [final-isolation.json](final-isolation.json) |
| Независимый host-оракул с ASan/UBSan | PASS, 500 карт, 2 040 000 сравнений страниц | [driver](review/oracle-final.c), [лог](review/oracle-final.log) |
| Чистые host-проверки x86 collector/walker | PASS, 149 проверок с ASan/UBSan | [лог](review/x86-host.log) |
| T-P1.5 Android/Termux | **SKIPPED** — отдельный запуск владельца не получен | Команды ниже |
| VirtualBox и физические устройства | **SKIPPED** — не запускались | Не блокируют эту QEMU-приёмку |

47 этапов — восемь host-наборов, две повторные сборки и сравнение, девять сборок вариантов и 27 QEMU jobs. ARM64-only — восемь host-наборов, девять сборок и 18 jobs. В итоговых JSON обоих выполненных наборов нет FAIL или SKIPPED. 138 Python unittest — покрытие одного host-этапа, а не 138 загрузок.

Среда: Linux x86-64, Python 3.12.14, GCC 14.2.0, Clang/LLD 19.1.7, QEMU 10.0.13 и внешний OVMF. Версии — [versions.log](versions.log); UTC, команды, недоступные пути ARM-only и SHA-256 firmware — [environment.json](environment.json). Бинарники, toolchain и firmware не включены в Git.

```sh
python3 tools/test.py --all --output build/t-p1-5
```

## Новые реальные сценарии

| Сценарий | Статус | UART / JSON |
|---|---|---|
| x86-64/OVMF, план на унаследованном CR3 | PASS | [UART](qemu-x86_64-ownership.log), [JSON](qemu-x86_64-ownership.json) |
| ARM EL1, 128 МиБ, MMU off | PASS | [UART](qemu-aarch64-ownership.log), [JSON](qemu-aarch64-ownership.json) |
| ARM EL2 → EL1, 128 МиБ, MMU off | PASS | [UART](qemu-aarch64-ownership-el2.log), [JSON](qemu-aarch64-ownership-el2.json) |
| ARM EL1, 256 МиБ, MMU off | PASS | [UART](qemu-aarch64-ownership-256m.log), [JSON](qemu-aarch64-ownership-256m.json) |
| x86, контролируемый неподтверждённый IMAGE claim | PASS: ожидаемый отказ | [UART](qemu-x86_64-ownership-invalid.log), [JSON](qemu-x86_64-ownership-invalid.json) |
| ARM EL1, тот же отказ | PASS: ожидаемый отказ | [UART](qemu-aarch64-ownership-invalid.log), [JSON](qemu-aarch64-ownership-invalid.json) |
| ARM EL2 → EL1, тот же отказ | PASS: ожидаемый отказ | [UART](qemu-aarch64-ownership-invalid-el2.log), [JSON](qemu-aarch64-ownership-invalid-el2.json) |

Положительный результат требует полного handoff/MEMMAP-префикса, архитектурного состояния, полного набора claims, согласованных raw walks, точных PROTECTED/REVIEW, нулевой выдачи и обычного halt после `OWNERSHIP PASS`. Раннер заново выводит бинарное доказательство и связывает его с manifest, режимом, размерами и хешами реально загруженных артефактов.

Отрицательный профиль меняет только `claims[0].proof` обязательного IMAGE в собственной metadata. Handoff, UEFI map и DTB не портятся. Все три отрицательных UART содержат после MEMMAP единственный ownership-итог `STYLEOS OWNERSHIP ERROR: required`, без частичного плана, PASS или обычного halt. JSON содержит `ownership_plan=null` и `refusal=required`.

Во всех семи случаях VM оставалась живой в окне наблюдения, затем runner сам остановил QEMU: `requested_stop=True`, `natural_exit=False`, `settled=True`. SIGTERM в конце — ожидаемое действие runner. Пустой журнал, один PASS, timeout, reset и частичная публикация не засчитываются.

## Как читать результат

| Статус | Значение | Право использовать страницу |
|---|---|---|
| PROTECTED | Полная страничная защита известного живого объекта или базового резерва; источник и причина заданы policy/claims | Не выдавать; это не утверждение о правах отображения |
| REVIEW | Диагностический кандидат после вычитания защиты, с причиной `no-transfer` | Не читать пробами, не писать, не обнулять и не выдавать |
| UNCLASSIFIED | За пределами заявленного покрытия; все дыры перечислять не требуется | Никакого положительного вывода |
| GRANTED | Реально переданные страницы | **Всегда 0** в T-P1.5 |

PROTECTED — объединение округлённых наружу базовых RESERVED и известных claims, без повторного счёта вложенных объектов. REVIEW округляется внутрь и вычитается из этого объединения. Защита имеет приоритет. Исходные данные не заменяются объёмом из командной строки QEMU.

UEFI-политика допускает в кандидаты только ConventionalMemory с WB и поддерживаемыми атрибутами. Loader/Boot Services/Runtime Services, неизвестные типы и неподходящие атрибуты остаются защищены. ARM использует проверенные memory-узлы DTB, весь DTB, reserve map и поддерживаемые статические `/reserved-memory`; `no-map`/`reusable` не отменяют защиты. Неподдерживаемый источник или противоречивое доказательство вызывает отказ.

## Полное размещение и граница записи

Новый постоянный mutable object — только `styleos_ownership_workspace`, **65536 байт**, выровненный по 4096 внутри BSS образа. Перед первой записью проверены полный image/reserved extent и исключение стека, handoff, источника, diagnostic workspace, старого pool и metadata. Обычные C locals находятся на уже принадлежащем образу стеке. Адреса, перечисленные в плане, не используются для пробных чтений, очистки, аллокации или освобождения.

| Положительный профиль | Образ `[start,end)` | Workspace плана | Workspace диагностики |
|---|---|---|---|
| x86 | `0x6005000–0x6041000` | `0x601e000–0x602e000` | `0x6011060–0x60140d0` |
| ARM EL1/EL2 | `0x40080000–0x400b6000` | `0x40096000–0x400a6000` | `0x40089000–0x4008c070` |

[Бинарное доказательство x86](sites/ownership/x86_64-ownership-site.json) проверяет PE sections, весь virtual tail, SizeOfImage и runtime ImageBase + RVA. [ARM proof](sites/ownership/aarch64-ownership-site.json) проверяет точные symbol sizes, NOBITS, writable identity PT_LOAD, BSS clear bounds и объявленный полный Image size. Отдельно связаны diagnostic, stack, pool/metadata и IDT/handoff либо vectors. Нет подгонки под адреса этого прогона: новые координаты выводятся из каждой сборки и сверяются с runtime.

Boot pool присутствует как защищённый объект, но **не admitted** и не используется. Paging tables/control/data T-P1.4 в этом профиле **не собраны**. UART/JSON прямо фиксируют оба состояния.

## x86: известные зависимости и честная неполнота

CR3 до и после — **`0x7801000`**, без переключения. Защищены root и три известные table pages `0x7801000`, `0x7802000`, `0x7803000`; root входит в этот частичный набор и не считается дважды. GDT VA `0x75dc000` переведён walker в PA `0x75dc000`, а IDT занимает полный не выровненный на страницу интервал `0x603e010–0x603f010` внутри образа. Равенство GDT VA/PA в этом OVMF — измеренный результат, а не общее предположение.

Каждая физическая table page до чтения должна уже целиком быть RESERVED и вне CANDIDATE. Читаются только активный root и достигнутые от него descriptor children, при явной узкой UEFI physical-access предпосылке и bounded identity closure. UART содержит четыре raw paths: один GDT и три TABLE identity traces. Core и runner независимо воспроизводят арифметику, проверяют root/ширину/уровни/PA/PAT и согласованность общих слотов; только аппаратные A/D bits могут различаться.

Это **не полный обход дерева** и не дамп всех image leaves. Все image pages проверяются adapter по ограниченному профилю, а raw traces подтверждают заявленные GDT/частичные TABLE зависимости. Поэтому обязательны `UNRESOLVED flags=3`: `inherited-table-set-incomplete` и `other-firmware-state`. PASS означает полноту именно заявленных защит и явное обозначение неопределённости. Ни одна REVIEW-страница не получает положительного вывода о передаче.

В этом прогоне PROTECTED насчитывает 3 224 345 страничных единиц, REVIEW — 20 615, GRANTED — 0. **PROTECTED включает не-RAM резервы и MMIO-апертуры UEFI; это не объём установленной RAM.** Нельзя складывать эти счётчики и объявлять результат ёмкостью гостевого ОЗУ.

## ARM: EL1/EL2, DTB и два объёма RAM

Оба старта приходят к прежнему EL1h с MMU off: SCTLR `0xc50838`, VBAR `0x40080800`, DAIF `0x3c0`. `root=0` означает отсутствие активной stage-1 трансляции, а не чтение или очистку TTBR0. Vectors и PL011 `0x9000000–0x9001000` защищены отдельными claims; PL011 не объявляется RAM.

| Вход из DTB | Полный DTB | PROTECTED pages | REVIEW pages | GRANTED |
|---|---|---:|---:|---:|
| RAM `[0x40000000,0x48000000)`, 128 МиБ | `[0x44000000,0x44100000)` | 311 | 32 458 | 0 |
| RAM `[0x40000000,0x50000000)`, 256 МиБ | `[0x48000000,0x48100000)` | 311 | 65 226 | 0 |

Прибавилось 32768 REVIEW pages = 128 МиБ, но разрешение на выдачу не появилось. FDT-резервы и полный проверенный source сохранены. В SOURCE поля stride/version=0 обозначают неприменимость UEFI-полей, а не версию DTB 0. `unresolved=0` относится только к заявленному MMU-off QEMU набору и не доказывает передачу RAM или поддержку нативного телефона.

## Отказы, регрессии и независимое review

[review.md](review.md) не содержит открытых дефектов для этого узкого профиля. Помимо 149 x86 host checks и независимого bitmap oracle, отдельный replay перепроверил все семь окончательных UART/JSON через interval events и descriptor arithmetic: [driver](review/actual-evidence.py), [результат](review/final-actual-evidence.log). Эти host-проверки и fake subprocess tests не выдаются за новые QEMU-загрузки.

Покрыты вложенные/пересекающиеся резервы, частичные страницы и хвосты, overflow/count limits, неизвестный источник/защищённый неизвестный тип, потеря обязательного claim, отсутствие доказательства, raw-walk противоречия, ложный PASS, REVIEW вне RAM и частичный результат. Все 28 пар восьми флагов взаимоисключающие; старые варианты не содержат нового кода/буфера.

Первоначальная host-проверка с GCC `-O2 -Werror` выявила предупреждение о локальном GDT range. Инициализация исправлена до итоговой приёмки; [первый FAIL сохранён](initial-host-check/host-ownership.log). Окончательные C-наборы, independent ASan/UBSan oracle и оба полных набора прошли на исправленном коде. Допущены только различия A/D между raw snapshots; остальные изменения descriptor PA/прав/типа отвергаются.

Сохранены шесть paging-сценариев T-P1.4, UD2/BRK и PC proof, boot pool, normal/negative handoff/map, оба ARM EL и 256 МиБ. Все **28 артефактов семи старых профилей** побайтно совпали с базой при этом toolchain, а обычные четыре — ещё и между двумя независимыми сборками. Это фактическая сверка, не обещание идентичности при другом компиляторе.

[tested-sources.json](tested-sources.json) фиксирует 71 исходный файл/инструмент. [Scope review](review/scope-review.json) подтверждает 382 побайтно сохранённых базовых source/test/исторических evidence файлов, исключая разрешённые integration/build/runner/mode tests и текущую документацию. Старые `boot_info`, normalizer/parsers, pool, startup, GDT/IDT/VBAR/exception и paging paths не менялись.

## Команды владельцу для Termux

ARM64-only Linux запуск выполнен с недоступными `OVMF_CODE`, `OVMF_VARS`, `LLD_LINK`, `QEMU_X86` из environment.json. Он прошёл 35/35 и не использовал эти инструменты. Это не Android PASS. Новый подтверждённый отчёт телефона T-P1.4 не получен; прежний T-P1.3 20/20 сюда не переносится.

```sh
pkg install git python clang lld qemu-system-aarch64-headless
git clone --branch feature/styleos-t-p1-5 --single-branch https://github.com/LPYURA2/OS.git styleos-tp15
cd styleos-tp15
git checkout --detach a284c30abbea2999eeaef721e0bd380c582c6dd0
git rev-parse HEAD
qemu-system-aarch64 --version
CC=clang python tools/test.py --host --qemu --qemu-arch aarch64 --output build/termux-tp15
echo $?
```

Ожидаются exit 0 и 35 PASS; фактический статус устанавливается по устройству. Если имя QEMU-пакета отличается, используйте пакет `qemu-system-aarch64` из своих репозиториев Termux. Root и изменение разделов телефона не нужны; runner сам останавливает VM.

Сохраните `build/termux-tp15/test-results/results.json`, `host-ownership.log`, `host-python.log`, `build-ownership.log`, `build-ownership-invalid.log`, все `qemu-aarch64-ownership*.log` и соответствующие `.json`. Лучше передать полный каталог test-results для регрессий, плюс SHA/version и manifest из `build/termux-tp15/ownership` и `ownership-invalid`.

## Вывод и остановка

PROTECTED — «нужно сохранить», REVIEW — «можно обсуждать будущую передачу», GRANTED — «передача доказана и разрешена». T-P1.5 даёт только первые два вида сведений и всегда нулевой третий. Даже после собственного CR3 старые таблицы, GDT и источники не становятся свободными автоматически; будущая интеграция должна отдельно защищать активные T-P1.4 table/control/data и обосновывать оставшиеся зависимости. Контракт владельца/источника/срока — [ADR-0008](../../adr/0008-physical-memory-ownership-plan.md).

Main остаётся `dfc5be9c2f699928a42903e70066f1bd8b20f65a`, Portable-os main — `bf8570b35ebb6cb59f319a33088e7075b1d6ddc4`; разрешена публикация только новой task-ветки, без изменения прежних refs, merge, force push или release. PMM, общий RAM pool, reclaim, собственная GDT и выбор модели ядра не начаты. После отчёта этап остановлен.
