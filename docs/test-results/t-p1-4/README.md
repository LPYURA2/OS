# T-P1.4 — собственные таблицы, alias и аппаратный translation fault

База: [`877f5e2bb85d484852a627e3a76a4bb956b40d50`](https://github.com/LPYURA2/OS/commit/877f5e2bb85d484852a627e3a76a4bb956b40d50). Проверенная реализация: [`68314ee6be30096e70267e6b003fd4cc771dba0e`](https://github.com/LPYURA2/OS/commit/68314ee6be30096e70267e6b003fd4cc771dba0e). Ветка — [`feature/styleos-t-p1-4`](https://github.com/LPYURA2/OS/tree/feature/styleos-t-p1-4). Отчётный коммит добавляет только документацию, свидетельства и закреплённые команды; проверенный код не меняется.

## Результат по средам

| Проверка | Статус | Свидетельство |
|---|---|---|
| База T-P1.3 до изменения существующих исходников | PASS, 28/28 | [JSON](baseline/results.json), [console](baseline/console.log) |
| Полный T-P1.4 в Linux | **PASS, 37/37 этапов**, exit 0 | [results.json](results.json), [console](full-console.log) |
| Host Python внутри полного набора | **PASS, 101/101 unittest** | [Журнал](host-python.log) |
| ARM64-only в Linux без доступных OVMF/x86 linker/QEMU | **PASS, 27/27**, exit 0 | [JSON](arm-only/results.json), [console](arm-only/console.log) |
| Две независимые сборки четырёх обычных артефактов | PASS, побайтно одинаковы | [reproducibility.log](reproducibility.log) |
| Пять прежних профилей без paging-кода и резерва | PASS | [final-isolation.json](final-isolation.json) |
| Финальные C-проверки ASan + UBSan | PASS, core/admission/x86 | [core](final-sanitize-core.log), [admission](final-sanitize-admission.log), [x86](final-sanitize-x86.log) |
| T-P1.4 на Android/Termux владельца | **SKIPPED** — новый прогон ещё не получен | Команды ниже |
| VirtualBox и физические устройства | **SKIPPED** — не запускались | QEMU не подменяет эти проверки |

37 этапов — семь host-наборов, две повторные сборки и сравнение, семь сборок вариантов и 20 QEMU jobs. ARM64-only — семь host-наборов, семь сборок и 13 jobs. В обоих итоговых JSON нет FAIL или SKIPPED. 101 Python unittest — внутреннее покрытие одного host-этапа, а не ещё 101 загрузка.

Среда: Linux x86-64, Python 3.12.14, GCC 14.2.0, Clang/LLD 19.1.7, QEMU 10.0.13, внешний OVMF. Фактические версии — [versions.log](versions.log); UTC, точные команды, недоступные ARM-only пути и SHA-256 firmware — [environment.json](environment.json). Бинарники, firmware и toolchain в Git не включены.

```sh
python3 tools/test.py --all --output build/t-p1-4
```

## Шесть новых реальных сценариев

| Сценарий | Итог | UART и JSON |
|---|---|---|
| x86-64/OVMF: свои таблицы и alias RW | PASS | [UART](qemu-x86_64-paging.log), [JSON](qemu-x86_64-paging.json) |
| x86-64/OVMF: намеренный unmapped read | PASS, один #PF | [UART](qemu-x86_64-paging-fault.log), [JSON](qemu-x86_64-paging-fault.json) |
| ARM64 initial EL1: свои таблицы и alias RW | PASS | [UART](qemu-aarch64-paging.log), [JSON](qemu-aarch64-paging.json) |
| ARM64 initial EL1: unmapped read | PASS, один Data Abort | [UART](qemu-aarch64-paging-fault.log), [JSON](qemu-aarch64-paging-fault.json) |
| ARM64 initial EL2 → EL1: alias RW | PASS | [UART](qemu-aarch64-paging-el2.log), [JSON](qemu-aarch64-paging-el2.json) |
| ARM64 initial EL2 → EL1: unmapped read | PASS, один Data Abort | [UART](qemu-aarch64-paging-fault-el2.log), [JSON](qemu-aarch64-paging-fault-el2.json) |

Каждый новый образ сначала проходит прежние handoff и MEMMAP. После аппаратного переключения проверяются собственный root, регистры режима и **каждая 4-КиБ leaf**. Алиас `0x0000400000000000` читает `0x123456789abcdef0`; запись `0xfedcba9876543210` через него видна по исходному адресу. Алиас отсутствовал в старых таблицах x86; на ARM старый профиль был MMU-off, а alias не был физическим адресом data. Это реальное действие трансляции, а не только успешная запись регистра.

Положительный вариант требует `STYLEOS PAGING PASS`, затем обычный `STYLEOS KERNEL HALT`. Отрицательный после того же доказанного перехода требует READY, одну аппаратную exception-запись и `STYLEOS EMERGENCY HALT`, без PAGING PASS и обычного halt. Во всех шести процессах harness подтвердил живую VM после терминального маркера и сам остановил её: `requested_stop=True`, `natural_exit=False`, `settled=True`. SIGTERM в конце ожидаем; timeout/reset/тишина не принимаются.

## Резерв и фактические адреса

Резерв — 32 страницы таблиц, отдельные 4096 байт control и 4096 байт data. Он целиком принадлежит образу и присутствует только в двух новых вариантах. Boot pool не используется и не меняется. На всех шести испытаниях реально потребовалось **9 страниц таблиц**, а не широкие block/huge mappings ради экономии страниц.

| Профиль | Образ `[start,end)` | Резерв таблиц | Фактический root |
|---|---|---|---|
| x86 positive | `0x5ff6000–0x6042000` | `0x600e000–0x602e000` | `CR3=0x600e000` |
| x86 fault | `0x5ff5000–0x6041000` | `0x600d000–0x602d000` | `CR3=0x600d000` |
| ARM positive/fault, EL1/EL2 | `0x40080000–0x400c7000` | `0x40096000–0x400b6000` | `TTBR0=0x40096000` |

[paging-evidence.json](paging-evidence.json) фиксирует также полные control/data/workspace/stack/source bounds. Переносимые координаты и хеши: [positive x86](sites/paging/x86_64-paging-site.json), [fault x86](sites/paging-fault/x86_64-paging-site.json), [positive ARM](sites/paging/aarch64-paging-site.json), [fault ARM](sites/paging-fault/aarch64-paging-site.json). В PE таблицы имеют RVA `[0x18000,0x38000)` и целиком укладываются в writable virtual tail и `SizeOfImage=0x4c000`; runtime-координаты вычислены из реальной базы. Preferred `0x140000000` не подставлялся вместо неё.

ARM: таблицы/control полностью в NOBITS, writable identity PT_LOAD и BSS `[0x40089000,0x400c7000)`. Объявленный Image memory size — `0x47000`, включая BSS; raw file-backed байты сверены с ELF. Старый цикл очистки BSS не изменён.

Admission проверяет полные диапазоны до первой записи в экспериментальные данные, исключает стек/handoff, UEFI-карту или DTB, workspace, старый pool и его metadata. Построитель проверяет VA/PA, ширину, выравнивание, права/тип, конфликт, лимит и отсутствие probe. Любой отказ возвращает `root=0`; частичные таблицы не активируются. Бинарное доказательство заново выводится из реальных файлов и связывается с manifest, режимом, размерами и SHA.

## x86: GDT и настоящий #PF

Фактический профиль: 40-битный PA, четырёхуровневые таблицы, PCID=0, LA57=0, PGE=0, EFER `0xd00`, PAT `0x0007040600070406`. Старый CR3 — `0x7801000`. Код также учитывает очистку глобальных трансляций при поддержанном PGE=1, но именно эта ветвь состояния в текущем OVMF не наблюдалась. CR0.WP активен после перехода.

GDTR указывает VA `0x75dc000`, лимит даёт 72 байта; старый walker подтвердил PA `0x75dc000`. CS `0x38`, raw descriptor **`0x00af9a000000ffff`**, Accessed=0. Поэтому только его уже зарезервированная физическая страница вне CANDIDATE отображена RW/NX для возможного аппаратного обновления Accessed. Программных записей в дескриптор и новой GDT нет. Остальные GDT pages при наличии были бы RO/NX. Нужные IDT gates/handlers, стек и полный image сохраняются.

Первоначальная более строгая RO/A=1 предпосылка дала явный отказ до переключения. Этот результат **FAIL сохранён**, а не объявлен успешной загрузкой: [UART с raw descriptor](initial-refusal/x86-accessed-zero.log), [начальная интеграционная сводка](initial-refusal/integration-results.json). Финальная политика учитывает реальное требование CPU; итоговые шесть сценариев прошли после неё. Это не разрешение писать в общую RAM.

В отрицательном образе saved RIP `0x5ffb460` = фактическая база `0x5ff5000` + RVA `0x6460`. В executable PE проверены байты `48 8b 01` — `mov rax,[rcx]`. Журнал:

```text
STYLEOS EXCEPTION arch=x86_64 kind=sync vector=14 reason=page-fault pc=0x0000000005ffb460 error=0x0000000000000000 cr2=0x0000400000001000
STYLEOS EMERGENCY HALT
```

Error 0 означает supervisor read отсутствующей страницы, без protection/RSVD/instruction fault. Один аппаратный #PF подтверждает путь через сохранённые IDT/GDT под новым корнем. Это не recovery и не гарантия для сломанного стека, UART или произвольной firmware.

## ARM: режим и настоящий Data Abort

Оба старта дают одинаковый EL1 профиль: TTBR0 `0x40096000`, TTBR1=0, TCR `0x480903010`, MAIR `0x44`, SCTLR `0x30d00801`, VBAR `0x40080800`. TTBR0 stage-1 — 48-битный VA, четыре уровня, 4-КиБ granule; верхние walks отключены. Image/data — Normal-NC, PL011 `0x9000000–0x9001000` — отдельная Device-nGnRnE RW/NX страница. Кэши не включаются, VBAR/стек/DAIF сохраняются. Проверены DSB/TLBI/ISB и readback.

Намеренный probe `0x400000001000` каноничен для выбранного VA-режима, но не имеет leaf. Saved ELR `0x4008530c` совпадает с ELF/raw Image инструкцией `ldr x1,[x0]`, байты `01 00 40 f9`:

```text
STYLEOS EXCEPTION arch=aarch64 kind=sync class=0x25 reason=translation-fault pc=0x000000004008530c esr=0x0000000096000007 elr=0x000000004008530c spsr=0x00000000600003c5 far=0x0000400000001000
STYLEOS EMERGENCY HALT
```

EC `0x25` — Data Abort текущего EL; DFSC 7 — translation fault level 3; WnR=0 и FAR достоверен. Это не address-size или permission fault и не программная имитация.

## Регрессии и проверка отказов

Сохранены все старые normal/invalid-handoff/invalid-map, реальные UD2/BRK с PC/opcode proof, boot pool, ARM EL1/EL2 и 256 МиБ. [final-isolation.json](final-isolation.json) подтверждает отсутствие новых paging symbols, сообщений и объектов во всех пяти старых вариантах. Четыре обычных артефакта не только повторяемы между двумя новыми сборками, но и фактически совпали с базовым прогоном при этом toolchain; это наблюдение, а не обещание идентичности при смене компилятора.

Host покрывает границы/overflow/alignment, неполное резервирование и overlap, PA width, нехватку таблиц и root=0, неправильные права/тип/alias, недостающий leaf и unmapped probe. Чистые x86 fixtures проверяют неподдержанные регистры, дескрипторы, старые обычные/2-МиБ/1-ГиБ walks, отсутствие writable proof последней страницы и резервирование аппаратной GDT-записи. Host-ветка не исполняет privileged instructions и пригодна для ARM64-хоста.

Python повреждает whole-image proof, manifest, размеры/хеши, режимы и UART: пустой/PASS-only/timeout, неверный root, PTE/rights/type, alias, PC/relocation, syndrome/FAR/CR2, дубликаты и неверные terminal markers. Проверены все 15 пар взаимоисключающих шести флагов и отдельные каталоги. Lifecycle fake subprocesses отделены от настоящих загрузок.

61 исходный файл/инструмент зафиксирован в [tested-sources.json](tested-sources.json). [Предварительный scope audit](scope-audit.json) и [финальная изоляция](final-isolation.json) дополняют друг друга: 30 защищённых исходников и 12 прежних test-файлов побайтно сохранены. `boot_info`, normalizer/parsers, boot pool, старые startup/IDT/VBAR/exception paths не изменены. Исторические отчёты не переписаны.

[Первоначальное независимое review](review-initial.md) сохранено как исторический WIP, без выдачи его за завершённую финальную приёмку. Его замечания закрывались кодом и тестами; [финальная проверка основным инженером](review-final.md) сверяет уже окончательные исходники и реальные результаты, включая новый A=0 путь.

## Воспроизведение в Termux

Linux ARM64-only был реально выполнен с `OVMF_CODE`, `OVMF_VARS`, `LLD_LINK`, `QEMU_X86`, направленными на недоступные пути из environment.json. Он прошёл 27/27 и не обращался к этим инструментам. Это **не Android-прогон**. Подтверждённые 20/20 T-P1.3 на телефоне относятся к предыдущему коду и не засчитываются для T-P1.4.

Новый каталог и закреплённый SHA проверенных исходников:

```sh
pkg install git python clang lld qemu-system-aarch64-headless
git clone --branch feature/styleos-t-p1-4 --single-branch https://github.com/LPYURA2/OS.git styleos-tp14
cd styleos-tp14
git checkout --detach 68314ee6be30096e70267e6b003fd4cc771dba0e
git rev-parse HEAD
qemu-system-aarch64 --version
CC=clang python tools/test.py --host --qemu --qemu-arch aarch64 --output build/termux-tp14
echo $?
```

Ожидаются exit 0 и 27 PASS, но фактический статус определяют логи устройства. Если имя QEMU-пакета отличается, используйте пакет `qemu-system-aarch64` из своих репозиториев Termux. Root и изменение разделов телефона не нужны. Runner сам останавливает VM.

Передайте `build/termux-tp14/test-results/results.json`, `host-paging.log`, `host-python.log`, `build-paging.log`, `build-paging-fault.log`, четыре `qemu-aarch64-paging*.log` и соответствующие `.json`; полный каталог test-results лучше сохраняет регрессии. SHA, версия QEMU и `manifest.json` в обоих новых каталогах позволяют сопоставить результаты с кодом и артефактами.

## Что это означает и где остановились

Boot pool научил учитывать маленькие уже принадлежащие образу страницы. Для таблиц выделен отдельный конечный резерв: C-код строит в нём дескрипторы, проверяет их и только затем передаёт физический root CPU. Смена таблиц меняет перевод **VA → PA**, а не владельца страниц. UEFI/DTB, старые таблицы и прочие загрузочные структуры не освобождаются; общая RAM не выдаётся.

Image closure остаётся RWX, включая таблицы; W^X, изоляция, общий PMM/heap, IRQ, SMP, процессы, IPC, user mode и новая модель ядра не реализованы. x86 продолжает зависеть от действующей GDT без TSS/IST; ARM — только QEMU virt, не профиль телефона. [ADR-0007](../../adr/0007-owned-translation-tables-experiment.md) фиксирует карты до/после, предпосылки и ограничения.

Публикуется только task-ветка. Main остаётся `dfc5be9c2f699928a42903e70066f1bd8b20f65a`, Portable-os main — `bf8570b35ebb6cb59f319a33088e7075b1d6ddc4`; прежние refs не меняются. Без merge, force push и release. Следующий этап и архитектуру выбирает владелец после проверки отчёта; новая разработка за границами T-P1.4 не начата.
