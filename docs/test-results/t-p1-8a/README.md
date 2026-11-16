# StyleOS T-P1.8A — первый сохраняемый тестовый файл

**PASS:** машинный код гостя StyleOS создал `note.txt` на отдельном VirtIO
диске, подтвердил запись FLUSH и прочитал те же байты после остановки и нового
запуска QEMU. Обновление до другой версии также пережило новый запуск.
В отдельном сценарии SIGKILL после подготовки данных B, до новой фиксации,
оставил доступной целую зафиксированную A.

Одна команда сама собирает ARM boot image, создаёт два пустых тестовых диска
и выполняет семь независимых VM-фаз. Host не создаёт содержимое файла и не
исправляет диск между фазами. Это ограниченное kernel-only хранилище одного
файла для ARM QEMU, без запуска внешних программ или пользовательского file API.

## Коммиты и граница публикации

- База: [`2831e6a1c54562d1f37518913f07e648cc00cc2d`](https://github.com/LPYURA2/OS/commit/2831e6a1c54562d1f37518913f07e648cc00cc2d), отчётная вершина T-P1.7.
- Проверенная реализация: [`6dfb61d49835c50d561ca2b4a509563e3c744e59`](https://github.com/LPYURA2/OS/commit/6dfb61d49835c50d561ca2b4a509563e3c744e59).
- Следующий отдельный коммит содержит этот отчёт, текстовые свидетельства и
  закреплённый SHA в TESTING. Его SHA передан в итоговом сообщении; локально:
  `git log -1 --format=%H -- docs/test-results/t-p1-8a/README.md`.
- Публикуется только [`feature/styleos-t-p1-8a`](https://github.com/LPYURA2/OS/tree/feature/styleos-t-p1-8a).
  `main`, прежние refs, `docs/styleos-readme`, Portable-os, корневой README и
  исторические свидетельства сохранены. Нет merge, force push или release.

## Фактические результаты 16 ноября 2026 года

| Проверка | Статус | Свидетельство |
|---|---|---|
| Точная база, полный повтор тем же toolchain | PASS, 57/57, exit 0 | [results](baseline/results.json), [консоль](baseline-console.log); 219 Python tests, OK |
| Полный Linux регрессионный набор | PASS, 57/57, exit 0 | [results](regression-full/results.json), [консоль](full-console.log); 10 host, 3 reproducibility, 11 build, 33 прежних QEMU jobs |
| Linux ARM64-only без OVMF/x86 tools | PASS, 43/43, exit 0 | [results](regression-arm-only/results.json), [консоль](arm-console.log); 10 host, 11 build, 22 прежних ARM jobs |
| Python unittest в обоих регрессионных наборах | PASS, 261 tests, OK | [полный](regression-full/host-python.log), [ARM-only](regression-arm-only/host-python.log); один host-этап, не добавляется к 57/43 |
| Новый storage runner, первый вызов на точном SHA | PASS, 16/16, exit 0 | [results](storage-primary/test-results/results.json), [консоль](storage-primary/console.log), [provenance](storage-primary/provenance.json) |
| Та же команда с тем же output, второй вызов | PASS, 16/16, exit 0 | [results](storage-repeat/test-results/results.json), [консоль](storage-repeat/console.log), [provenance](storage-repeat/provenance.json) |
| Сохранность первого прогона при повторе | PASS, 59/59 файлов неизменны | [rerun-preservation.json](rerun-preservation.json), включая оба диска, образы и свидетельства; бинарники в Git не включены |
| Все прежние артефакты с T-P1.7 включительно | PASS, 44/44 идентичны базе | [old-artifacts.json](old-artifacts.json), 11 прежних профилей × четыре артефакта; STORAGE marker отсутствует |
| Обычная воспроизводимость | PASS, четыре артефакта | [reproducibility.log](regression-full/reproducibility.log), независимые сборки |
| Отдельный storage ELF и raw Image | PASS, оба совпали между двумя вызовами | SHA в двух storage manifests/provenance, независимая финальная проверка |
| Обычный ARM-образ с записываемым тестовым диском | PASS, диск не изменён | [JSON](ordinary-disk-guard/ordinary-disk-guard.json), [сырой UART](ordinary-disk-guard/ordinary-disk-guard.uart.log); даже storage bootarg не активирует запись |
| Android/Termux T-P1.8A | SKIPPED | Контрольный прогон владельца нового SHA ещё не получен |
| VirtualBox T-P1.8A | SKIPPED | Новый storage-профиль предназначен для ARM QEMU; x86-драйвер в этап не входит |
| Физический накопитель / загрузка на железе | SKIPPED | Не испытывались; используются только runner-created обычные файлы |

В финальных наборах FAIL нет. Регрессионные 57/43 сохраняют прежние сценарии;
новые дисковые VM запускает отдельный storage runner. Его 16 этапов: три C host
модели и один Python-набор, сборка, binary proof, семь VM, проверка неизменности
исходников и две сквозные проверки. Storage Python содержит 38 tests
(26 runner и 12 site), входит в один этап и также включён в регрессионные 261.
Два вызова повторяют те же проверки; они не считаются 32 разными сценариями.

## Семь независимых VM

Версии и имя зашиты в guest: `note.txt`, A=`StyleOS note A\n` (15 байт),
B=`StyleOS note B: updated\n` (24 байта). Host передаёт только фазу через DTB
bootargs `styleos.storage=...`. Чтение принимается после точного guest-сравнения
имени, поколения, длины и каждого байта.

| VM | Действие гостя и результат | READ / WRITE / FLUSH |
|---|---|---|
| `persist-init-a` | Проверил все 2048 нулевых секторов, явно инициализировал диск, зафиксировал A поколения 1 | 2053 / 6 / 4 |
| `persist-read-a` | Новый процесс открыл тот же диск и прочитал целую A | 14 / 0 / 0 |
| `persist-update-b` | Прочитал A, подготовил и зафиксировал отличающуюся B поколения 2 | 19 / 5 / 3 |
| `persist-read-b` | Новый процесс прочитал именно B, а не старую A | 14 / 0 / 0 |
| `crash-init-a` | На отдельном свежем диске зафиксировал A | 2053 / 6 / 4 |
| `crash-prepare-b` | Прочитал A, подтвердил инвалидизацию неактивной фиксации и запись данных B; остановился до нового commit | 14 / 4 / 2 |
| `crash-recover-a` | После SIGKILL предыдущего QEMU новый процесс прочитал целую A | 14 / 0 / 0 |

Все семь первичных [UART/JSON](storage-primary/test-results/) и семь повторных
[UART/JSON](storage-repeat/test-results/) сохранены. `.uart.log` — точная копия
сырых UART-байтов; её SHA совпадает с `uart_sha256` в JSON. `.log` дополнительно
содержит команду и наблюдение harness. Компактная сводка PID, данных, счётчиков
и хешей дисков — [scenario-summary.json](scenario-summary.json).

Оба финальных вызова выполнены с `git_head` коммита реализации,
`git_dirty=false`, неизменными 110 source hashes, boot image, manifest и
sidecars. Получены 14 разных PID. Обычные 12 завершений наблюдались ещё 200 мс
после полного PASS+HALT, затем SIGTERM дал exit 0. Только два подтверждённых
`BARRIER BEFORE_COMMIT`, также наблюдавшихся 200 мс, разрешили SIGKILL с кодом
`-9`. Guest на этом барьере навсегда прекращает операции и не выдаёт PASS.
Неверный журнал, error, timeout, естественный exit/reset, неполный барьер или
одна строка PASS отвергаются.

В каждой цепочке один и тот же path/inode диска сохраняется между VM;
`disk_after` предыдущей фазы совпадает с `disk_before` следующей. Read-only
фазы сохраняют SHA диска. Новый boot не использует snapshot/restore. Наличие
B в RAM первого эмулятора не служит свидетельством сохранения.

## Формат и порядок фиксации

Диск ровно 1048576 байт, 2048 секторов по 512. Хранится один именованный файл:
имя 1..31 печатных ASCII-байт без разделителей пути, данные 0..1024 байт.
Все поля явно little-endian; формат не зависит от C struct padding.

| LBA | Назначение |
|---|---|
| 0 | Superblock версии 1 с геометрией, magic и CRC32 |
| 1 | Не используется |
| 2 / 3–4 / 5 | Metadata / payload 1024 байта / commit первой области |
| 6 / 7–8 / 9 | Metadata / payload / commit второй области |
| 10–2047 | Не используются хранилищем; при явной инициализации также должны быть нулевыми |

Metadata и commit связывают generation, имя/длину и контрольные суммы;
payload имеет CRC32 с нулевым дополнением. Mount только читает: принимает
целую зафиксированную версию, выбирает старую годную при повреждённой новой,
отказывает при отсутствии годной. Равные поколения и переполнение отвергаются.
Нераспознанный/повреждённый диск не форматируется. Только явная init-фаза
после полного нулевого сканирования пишет superblock и подтверждает FLUSH.
Точные magic, offsets и CRC-параметры — [ADR-0011](../../adr/0011-persistent-test-file-storage.md).

Каждое сохранение использует три подтверждённых шага:

1. Обнулить старый commit неактивной области и подтвердить FLUSH.
2. Записать metadata и два payload-сектора, подтвердить FLUSH. Теперь разрешён
   тестовый барьер; новая версия ещё не зафиксирована.
3. Записать новый commit и подтвердить финальный FLUSH. Только теперь save успешен.

Отдельная предварительная инвалидизация нужна при повторе после fallback:
повреждённая B могла оставить корректную старую отметку поколения 2. Без её
упорядоченного обнуления восстановление тех же данных могло сделать эту
отметку снова действительной до нового commit. Это исправлено и покрыто
регрессией. При ошибке WRITE/FLUSH store и token становятся непригодны;
неопределённый результат нельзя объявлять сохранённым или повторять старым token.

CRC32 обнаруживает проверяемые повреждения; это не криптографическая защита.
Модели проверяют также частичное/переупорядоченное сохранение и ошибки flush,
но реальные VM испытывают выбранный управляемый барьер. SIGKILL QEMU не равен
отключению питания телефона/host или отказу физического накопителя.

## Устройство, память и виды проверок

Новый стенд: AArch64 `virt`, `cortex-a57`, TCG, один CPU, 128 МиБ,
современный VirtIO MMIO v2, одна polling split queue из восьми descriptors,
без IRQ-handler. DTB даёт 32 допустимых span в `[0x0a000000,0x0a004000)` с
размером/шагом `0x200`; выбранное реальное block device — `0x0a003e00`.
Согласованы только VERSION_1 и FLUSH: `0x0000000100000200`; отсутствие FLUSH,
RO, неверная вместимость, статус, used id/length, config или timeout дают отказ.

Очередь, request/bounce и driver control занимают по 4096 байт; guest control
и workspace — ещё две отдельные страницы. Все пять выровнены на 4096,
полностью входят в RESERVED Image и исключены из прочих живых объектов.
До инициализации storage-профиля после handoff и передачи буферов DMA
проверены EL1, MMU/cache off, little-endian, masked DAIF,
фиксированный Image и его полные границы. Новый binary proof проверяет
ELF/PT_LOAD/BSS/raw Image и entry независимо от UART. Произвольный указатель
не объявляется PA. Порядок доступности descriptors/avail/used обеспечивается
барьерами, количество запросов и polling ограничены.

Общая RAM не выдаётся: нет CANDIDATE/REVIEW, общего PMM/heap, reclaim или
использования старого boot pool. `GRANTED=0` сохраняет смысл. MMU/vectors,
boot_info v1, нормализация и старые пути загрузки не менялись. Обычные и
прежние профили не включают storage-код; новый x86 storage-драйвер не создавался.

| Где проверялось | Что фактически проверено |
|---|---|
| Реальный QEMU | Negotiation/MMIO/DMA READ/WRITE/FLUSH, guest init, A/B между независимыми процессами, барьер до commit и восстановление A, обычный образ без записи |
| C-модель формата | 17 групп: signatures, bounds/length/CRC, старая годная при новой повреждённой, отсутствие годной, overflow/token, fallback/reprepare, шесть границ сбоя и 12 случаев неопределённости I/O |
| C-модель транспорта | Отсутствующие features/RO, queue/capacity/range, status/used/config, timeout, finite request limit; дополнительные GCC/Clang/sanitizer проверки |
| C-модель FDT | Неверные/дублированные поля, фаза, compatible/reg/status, лимиты, повреждённые входы и отказ без изменения output |
| Python | Подмена образов/manifest/flags/границ, неверные UART/байты/flush-порядок, диски/пути, fakeprocess timeout/error/reset/ложный барьер и безопасная повторяемость |

GCC14 ASan+UBSan проверки формата, транспорта и FDT — PASS. Дополнительный
Clang sanitizer — SKIPPED: в пользовательском toolchain нет libclang_rt ASan;
проверки Clang19 без sanitizer прошли.

Ошибки устройства в моделях не объявляются реальными отказами QEMU-диска.
Source/model audits и sanitizer logs находятся в [review/](review/).

## Файлы для изучения

| Файл | Роль |
|---|---|
| [arch/aarch64/virtio_block.c](../../../arch/aarch64/virtio_block.c), [header](../../../include/styleos/virtio_block.h) | MMIO, features, очередь, DMA, polling, READ/WRITE/FLUSH и ошибки транспорта |
| [boot/aarch64/storage_fdt.c](../../../boot/aarch64/storage_fdt.c), [demo header](../../../include/styleos/storage_demo.h) | Ограниченная фаза и device spans из проверенного DTB |
| [kernel/storage.c](../../../kernel/storage.c), [header](../../../include/styleos/storage.h) | Независимое от MMIO сериализованное хранилище, CRC, выбор версии и фиксация |
| [kernel/storage_demo.c](../../../kernel/storage_demo.c) | Допуск памяти, реальные guest-фазы, точные байты файла и неподвижный барьер |
| [tools/storage_site.py](../../../tools/storage_site.py) | Полные ELF/Image/buffer proofs без LLVM objdump |
| [tools/test_storage.py](../../../tools/test_storage.py) | Одна команда, свежие диски, семь VM, проверка UART/JSON/SHA и сохранность предыдущих прогонов |
| [tests/test_storage.c](../../../tests/test_storage.c), [transport](../../../tests/test_virtio_block.c), [FDT](../../../tests/test_storage_fdt.c), [runner](../../../tests/test_storage_runner.py), [site](../../../tests/test_storage_site.py) | Модели отказов и отрицательные проверки свидетельств |

Учебный разбор — [LEARNING_RU](../../LEARNING_RU.md). Python управляет сборкой
и VM; форматирование и содержимое `note.txt` записывает гостевой C-код.

## Одна команда в Termux

Из нового каталога, на точном SHA реализации:

```sh
pkg install git python clang lld qemu-system-aarch64-headless
git clone --branch feature/styleos-t-p1-8a --single-branch https://github.com/LPYURA2/OS.git styleos-tp18a
cd styleos-tp18a
git checkout --detach 6dfb61d49835c50d561ca2b4a509563e3c744e59
CC=clang python tools/test_storage.py --output build/termux-tp18a
```

Последняя строка — весь контрольный прогон. Создавать диск, выбирать фазы или
переносить файлы вручную не нужно. Повтор той же строки создаст новый
`run-<UTC>-<random>` и сохранит предыдущий. Сохраните выведенный путь RUN,
`run.json`, `provenance.json`, все `test-results/*.log`, `*.json`, `*.uart`,
`boot/manifest.json`, `boot/aarch64/storage-site.json`, `storage-compiler.json`,
SHA checkout и версии Python/Clang/LLD/QEMU. Диски и бинарники в Git не добавлять.

Финальные Linux storage-вызовы специально получили несуществующие OVMF_CODE,
OVMF_VARS, LLD_LINK, QEMU_X86 и LLVM_OBJDUMP: новый маршрут прошёл без них.
Для старых task/user-processes регрессий LLVM objdump по-прежнему нужен.
Точные команды и версии — [environment.json](environment.json): Linux x86-64,
Clang/LLD 19.1.7, GCC 14.2.0, Python 3.12.14, QEMU 10.0.13. Toolchain и
firmware находятся вне Git. Linux-прогон не присваивает Android статус PASS.

Владелец отдельно подтвердил **T-P1.7 Android/Termux: 43/43, exit 0,
QEMU 11.1.3**, SHA `af898029ff71892d8e3c6a3308263624af722130`, оба ARM
user-processes сценария PASS. Источник — три скриншота; сырые UART/JSON не
приложены. Это owner-reported дополнение прошлого этапа, без изменения его
исторических отчётов и checksums; T-P1.8A на телефоне остаётся SKIPPED.

## Целостность и остановка

[tested-sources.json](tested-sources.json) содержит 110 source hashes коммита
реализации; [scope-audit.json](scope-audit.json) — границу изменений и сохранность
1009 базовых файлов: 1003 неизменны, шесть обновлены в разрешённой области;
все 896 исторических файлов свидетельств сохранены.
[Финальный независимый аудит](review/final-proof-audit.md) проверил реальные
образы, UART/JSON, процессы, цепочки дисков и повторный прогон.
Контрольные суммы всех текстовых свидетельств:

```sh
cd docs/test-results/t-p1-8a
sha256sum -c SHA256SUMS
```

Перед публикацией refs сверяются повторно; после неё проверяются удалённый
SHA/tree и неизменность всех остальных OS/Portable-os refs. Снимки — в review.
Нет общей ФС/POSIX, пользовательского file API, внешних программ, desktop,
SDK, `.tea`, Deep Drop, x86 storage-драйвера или готового пользовательского образа.
После отчёта и task-branch публикации работа останавливается до приёмки 1.8A
владельцем. T-P1.8B не начат.
