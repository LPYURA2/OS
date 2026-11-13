# T-P1.3 — проверенный пул страниц внутри образа

Реализация: [`f10ee3313b7d8d428a3207e5bcfff6b93842be09`](https://github.com/LPYURA2/OS/commit/f10ee3313b7d8d428a3207e5bcfff6b93842be09). Принятая база: `07fe425a0d4d5c0b9fbb65f8a37b30924523ec81`. Этот отчёт и закреплённые команды добавлены отдельным коммитом без изменения проверенных исходников. Ветка — `feature/styleos-t-p1-3`; main и Portable-os не обновляются.

## Фактические результаты

| Проверка | Статус | Доказательство |
|---|---|---|
| База T-P1.2 до изменений, полный новый запуск | PASS, 23/23 этапа | [JSON](baseline/results.json), [console](baseline/console.log) |
| T-P1.3, `python3 tools/test.py --all --output build/t-p1-3` | **PASS, 28/28**, exit 0 | [results.json](results.json), [console](full-console.log) |
| Host Python в полном прогоне | **PASS, 71/71** | [host-python.log](host-python.log) |
| Повторная проверка окончательного Python-набора | PASS, 71/71 | [final-python.log](final-python.log) |
| ARM64-only без доступных OVMF/x86-инструментов, Linux | **PASS, 20/20**, exit 0 | [JSON](arm-only/results.json), [console](arm-only/console.log) |
| Пул x86-64/OVMF | **PASS** | [UART](qemu-x86_64-bootpool.log) |
| Пул ARM64 initial EL1 | **PASS** | [UART](qemu-aarch64-bootpool.log) |
| Пул ARM64 initial EL2 → EL1 | **PASS** | [UART](qemu-aarch64-bootpool-el2.log) |
| Четыре обычных артефакта в двух сборках | PASS, побайтно одинаковы | [reproducibility.log](reproducibility.log) |
| ASan + UBSan для C-пула | PASS | [Журнал](review-host/gcc-sanitize.log) |
| T-P1.3 на устройстве Android/Termux | **PASS, 20/20** | [Отдельный прогон владельца, QEMU 11.1.3](termux/README.md) |
| VirtualBox и физические x86/ARM устройства | **SKIPPED** — не испытывались | QEMU не заменяет эти испытания |

28 этапов — шесть host-наборов, две повторные сборки и сравнение, пять сборок вариантов и 14 QEMU jobs. ARM64-only — шесть host-наборов, пять сборок и девять QEMU jobs. Числа посчитаны по JSON, а не по числу строк PASS. FAIL и SKIPPED внутри этих двух выполненных наборов отсутствуют. 71 Python unittest — отдельный счётчик внутри host-этапа.

Среда: Linux x86-64, Python 3.12.14, GCC 14.2.0, Clang/LLD 19.1.7, QEMU 10.0.13; использован внешний OVMF из прежнего toolchain. [versions.log](versions.log) фиксирует версии программ, [environment.json](environment.json) — UTC, команды, недоступные пути ARM-only и SHA-256 CODE/VARS. Сборочные бинарники и firmware в Git не включены.

## Что реализовано и откуда берётся память

В `kernel/boot_pool.c` находятся восемь страниц по 4096 байт и отдельная 4096-байтная страница метаданных. Оба объекта уже входят в загруженный образ ядра. После успешных handoff и MEMMAP внутреннее свидетельство позволяет проверить полные полуинтервалы, резервирование всего образа, переполнение, выравнивание и исключение стека, handoff, UEFI-карты/DTB и всей рабочей области диагностики. Отказ не меняет пул; выдача всегда обнуляет страницу.

Это похоже на выдачу восьми ячеек из уже имеющегося собственного шкафа: пул учитывает занятость своих ячеек, но не приобретает всё помещение. `MEMMAP CANDIDATE` описывает диагностические кандидаты RAM; T-P1.3 не выдаёт и не записывает их. API возвращает адрес известного объекта ядра, а не переводит произвольный указатель в физический адрес. Основания совместимости адресов и ограничения — [ADR-0006](../../adr/0006-image-owned-boot-pool.md).

Самотест компилируется только с `--test-bootpool`, в отдельный образ/каталог. Обычные образы содержат зарезервированный буфер, но не вызывают тестовую выдачу. [flag-safety.json](flag-safety.json) фиксирует отсутствие PASS-строки и selftest-объекта в обычных артефактах; host CLI suite проверяет отдельные defaults и взаимоисключение всех четырёх тестовых флагов.

## Полные границы и реальные операции

[pool-evidence.json](pool-evidence.json) связывает три UART-журнала с независимо повторённым разбором PE/map или ELF/raw Image. Статические результаты: [x86](pool-sites/bootpool/x86_64.json), [ARM](pool-sites/bootpool/aarch64.json).

| Диапазон `[start, end)` | x86-64 runtime | ARM64 EL1 и EL2 |
|---|---|---|
| Загруженный образ | `0x601c000–0x6044000` | `0x40080000–0x400a4000` |
| Выдаваемый пул, 32768 байт | `0x6028000–0x6030000` | `0x4008b000–0x40093000` |
| Метаданные, 4096 байт | `0x6030000–0x6031000` | `0x40093000–0x40094000` |
| Рабочая область диагностики | `0x6024060–0x60270d0` | `0x40087000–0x4008a070` |
| Повторно выданная страница №3 | `0x602b000` | `0x4008e000` |

Для PE пул имеет RVA `[0xc000, 0x14000)`, метаданные — `[0x14000, 0x15000)`, `SizeOfImage=0x28000`. К **обоим** концам прибавлена фактическая база `0x601c000`. Preferred ImageBase `0x140000000` отличается и не используется как runtime-база. Проверен весь интервал в writable секции `.data`, включая её нулевой virtual tail.

Для ARM оба объекта полностью внутри writable NOBITS/PT_LOAD и BSS `[0x40087000, 0x400a4000)`. Raw Image header объявляет полный размер `0x24000`, включая BSS; его file-backed секции совпадают с ELF. Неизменённый startup очищает полный BSS по тем же linker symbols. Сравнение относится к фиксированному QEMU Image-профилю с MMU off.

Во всех трёх журналах восемь адресов уникальны, выровнены и покрывают ровно пул. Проверены нули каждого байта, volatile-запись и чтение разных шаблонов во всех восьми страницах, отказ девятого запроса, освобождение страницы №3, повторная выдача того же адреса с нулями и сохранность остальных страниц. Затем идут:

```text
STYLEOS BOOTPOOL RW PASS pages=8 bytes=4096
STYLEOS BOOTPOOL EXHAUSTED PASS
STYLEOS BOOTPOOL REUSE <адрес страницы №3> ZERO
STYLEOS BOOTPOOL PASS
STYLEOS KERNEL HALT
```

Одна строка PASS не принимается. Runner требует валидный настоящий MEMMAP-префикс, полные доказанные границы, точный порядок и состав записей, отсутствие ошибок/timeout/exception. Он заново разбирает текущие бинарники и сравнивает sidecar и все входные SHA. После полного terminal marker QEMU остаётся живым ещё 200 мс; затем harness его останавливает. Во всех реальных журналах `requested_stop=True`, `natural_exit=False`, `settled=True`. SIGTERM в конце — штатное действие runner.

## Сохранённые регрессии

Все прежние normal, invalid-handoff, invalid-map и exception jobs сохранены. Пройдены ARM 128/256 МиБ, normal EL2 и реальные `UD2`/`BRK` с проверкой saved PC по образу. [pc-evidence.json](pc-evidence.json) и [fault-sites](fault-sites) фиксируют новые адреса:

- x86: фактическая база `0x601d000` + RVA `0x51b2` = saved RIP `0x60221b2`, opcode `0f0b`;
- ARM EL1/EL2: saved PC/ELR `0x40083db0`, opcode `002420d4`, ESR `0xf2000120`, FAR absent.

Обычные четыре артефакта — [normal-artifacts.json](normal-artifacts.json); отдельные тестовые — [bootpool-artifacts.json](bootpool-artifacts.json) и [exception-artifacts.json](exception-artifacts.json). Хеши исходников — [tested-sources.json](tested-sources.json).

Независимое [review](review.md) не имеет открытых блокирующих замечаний. Закрыты найденные до приёмки ошибки проверки alignment evidence и чтения нулевой длины за концом PE-файла; адресная база x86 уточнена в ADR. Host fixtures и fake subprocesses в `review-host/` проверяют harness и не выдаются за загрузку ОС. В окончательном наборе pool-site имеет 13 методов, pool-output — 14, CLI — 5; первоначальный независимый review pool-site выполнялся ещё на 11 методах.

[publication-audit.json](publication-audit.json) подтверждает 24 побайтно сохранённых защищённых исходника относительно базы: `boot_info`, normalizer/parsers, EBS, startup/linker, UART и протокол исключений/IDT/VBAR. Разница телефонного `ce245251` и принятой базы — только документация. Исторические журналы T-P1.0–T-P1.2 не переписывались.

## ARM64-only и команда владельца

Фактически выполнено в Linux:

```sh
OVMF_CODE=/nonexistent/styleos-t-p1-3/OVMF_CODE \
OVMF_VARS=/nonexistent/styleos-t-p1-3/OVMF_VARS \
LLD_LINK=/nonexistent/styleos-t-p1-3/LLD_LINK \
QEMU_X86=/nonexistent/styleos-t-p1-3/QEMU_X86 \
CC=clang python tools/test.py --host --qemu --qemu-arch aarch64 --output build/termux
```

Маршрут не использовал отсутствующие x86 linker/QEMU или OVMF. Разбор PE fixtures на хосте не требует этих инструментов. Это Linux-результат, **не телефонный прогон**. Новый результат T-P1.3 на устройстве владельца сохранён [отдельно](termux/README.md) и не входит в результаты Linux.

Для нового каталога в Termux:

```sh
pkg update
pkg install git python clang lld qemu-system-aarch64-headless
git clone --branch feature/styleos-t-p1-3 https://github.com/LPYURA2/OS.git StyleOS-TP13
cd StyleOS-TP13
git checkout --detach f10ee3313b7d8d428a3207e5bcfff6b93842be09
CC=clang python tools/test.py --host --qemu --qemu-arch aarch64 --output build/termux
echo $?
```

Ожидается exit 0 и 20 PASS; фактический результат определяют журналы устройства. Закреплённый SHA содержит проверенные исходники; отчётный коммит меняет только документацию. Нужны `build/termux/test-results/results.json`, `host-python.log`, `host-pool.log`, `build-bootpool.log`, `qemu-aarch64-bootpool.log`, `qemu-aarch64-bootpool-el2.log` и оба exception-журнала. Runner сам завершает QEMU; вручную прерывать его не нужно. При отсутствии headless-пакета проверьте доступный пакет QEMU в своих репозиториях Termux.

## Границы этапа и остановка

Это один фиксированный пул для одного CPU, без heap, общего PMM, SMP и защиты от stale-указателей после повторной выдачи. Не изменены MMU, GDT, IDT, VBAR, `boot_info v1` и правила карты. Нет reclaim firmware, recovery, IRQ, процессов, драйверов или приложений. Выбор модели ядра не сделан.

Main остаётся `dfc5be9c2f699928a42903e70066f1bd8b20f65a`, Portable-os main — `bf8570b35ebb6cb59f319a33088e7075b1d6ddc4`; разрешена публикация только новой task-ветки. Force push, merge и release не выполняются.

Телефонная проверка T-P1.3 завершена. Следующее решение владельца — отдельное задание на основания владения общей RAM и собственные таблицы трансляции либо иное архитектурное направление. Реализация этого следующего этапа не начата.
