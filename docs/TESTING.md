# Проверка StyleOS T-P1.8A

Проверка разделяет C-логику на хосте, сборку целевых машинных форматов, побайтную повторяемость, связанное бинарное доказательство и настоящее выполнение CPU в QEMU. PASS одного уровня не заменяет остальные. Новый отдельный ARM-профиль T-P1.8A проверяет сохранение `note.txt` между независимыми процессами QEMU и старую целую версию после остановки до commit. [Фактические результаты и журналы T-P1.8A](test-results/t-p1-8a/README.md) публикуются отдельно. [T-P1.7](test-results/t-p1-7/README.md), [T-P1.6](test-results/t-p1-6/README.md) и прежние отчёты остаются историей точной базы; они не доказывают новый дисковый сценарий.

## Требования и выбор инструментов

Для полного прежнего набора нужны Python 3, Clang, `lld-link`, `ld.lld`; для target smoke — `qemu-system-x86_64`, `qemu-system-aarch64` и внешняя firmware OVMF x86-64. Make необязателен. Пакетирование GPT/FAT16 выполняется Python standard library, без `mtools` и монтирования образа.

Отдельный storage runner требует Python, host C compiler, Clang/LLD и `qemu-system-aarch64`; он не использует OVMF, `lld-link`, x86 QEMU или LLVM objdump. `--test-storage` поддерживает только AArch64 и имеет отдельный выход. Весь тест работает с созданными runner обычными файлами, без root, графики и реальных дисков.

Профили `--test-tasks` и `--test-user-processes` дополнительно требуют `llvm-objdump` с декодерами обеих выбранных ISA. Он проверяет весь связанный исполняемый код на отсутствие неподдержанного FP/SIMD, а не только флаги компилятора. Без этого инструмента связанное integer-only доказательство не создаётся. Старые профили не получают новую зависимость.

В Debian/Ubuntu инструменты предоставляют `python3 clang lld llvm make qemu-system-x86 qemu-system-arm ovmf`. Обычная установка выполняется владельцем или администратором окружения; сама сборка StyleOS не запрашивает root и не загружает пакеты.

Возможны пользовательские toolchain-каталоги. Выбор программ и firmware задаётся переменными:

| Переменная | Назначение |
|---|---|
| `CLANG` | host-исполняемый Clang с нужными target backends |
| `CC` | C-компилятор host-тестов, по умолчанию `cc` |
| `LLD_LINK` | LLD PE/COFF linker |
| `LD_LLD` | LLD ELF linker |
| `LLVM_OBJDUMP` | LLVM objdump для полного integer-only proof task/user-processes профилей |
| `QEMU_X86` | `qemu-system-x86_64` |
| `QEMU_AARCH64` | `qemu-system-aarch64` |
| `OVMF_CODE` | read-only OVMF CODE firmware для x86-64 |
| `OVMF_VARS` | соответствующий исходный шаблон VARS |

CODE и VARS должны подходить друг к другу по варианту firmware. Шаблон VARS не должен быть личным изменяемым NVRAM-диском владельца; для теста используется отдельная копия. OVMF в Git не хранится.

Пример выбора установленных 4-МиБ OVMF файлов (пути проверьте на своём дистрибутиве):

```sh
export OVMF_CODE=/usr/share/OVMF/OVMF_CODE_4M.fd
export OVMF_VARS=/usr/share/OVMF/OVMF_VARS_4M.fd
python3 tools/test.py --all --output build/t-p1-8a-regressions
```

## Точные команды

Из корня репозитория:

```sh
python3 tools/build.py --arch all --output build
python3 tools/test.py --host --output build
python3 tools/test.py --repro --output build
python3 tools/test.py --qemu --output build
# Либо один полный прогон:
python3 tools/test.py --all --output build/t-p1-8a-regressions
```

Код завершения test runner: **0** — все выбранные проверки прошли; **1** — есть FAIL; **2** — есть SKIPPED без FAIL. Отсутствующий обязательный QEMU/OVMF виден в сводке и не превращается в зелёный прогон. Если флаг набора не задан, запускается полный набор.

Семь новых storage-VM запускает отдельная команда `tools/test_storage.py`,
описанная ниже. Прогон прежних профилей через `tools/test.py --all` проверяется
отдельно и не заменяет дисковые сценарии.

Оболочки:

```sh
make             # обе архитектуры
make host-test   # host C/Python проверки
make repro       # повторяемость
make smoke       # QEMU сценарии
make test        # полный набор
```

Автоматические журналы находятся в `build/test-results/*.log`, сводка — `build/test-results/results.json`. Эти файлы относятся к указанному прогону и не публикуются автоматически как исходники. Для приёмки отбираются реальные журналы в `docs/test-results/` с описанием среды. Перезапуск с другим компилятором, firmware или параметрами QEMU является другим экспериментом.

## Что проверяет каждый уровень

### Host-тесты

`tests/test_boot_info.c` проверяет фиксированную раскладку и поведение `boot_info_validate()` на корректных и ошибочных данных: null, magic/version/size, архитектура, платформа, flags, параметры карты UEFI, DTB, EL и резерв. Они работают на host и не доказывают, что машинный старт передаёт именно такие данные.

`tests/test_uefi.c` подменяет интерфейсы firmware контролируемыми функциями: нормальная карта/выход, устаревший MapKey, рост карты, ошибки выделения/чтения/выхода, неверный дескриптор, чрезмерный размер и исчерпание ограниченных повторов. Host simulation firmware проверяет ветвления и числа вызовов; настоящую UEFI ABI-цепочку проверяет только x86 target smoke.

`tests/test_disk.py` проверяет сгенерированный GPT/FAT-диск и EFI boot path, в том числе обнаружение описанных тестом повреждений метаданных. Это тест формата упаковки, а не файловая система StyleOS.

`tests/test_boot_memory.c` проверяет нормализацию, лимиты, несовместимые диапазоны, правила UEFI и поддерживаемые/повреждённые DTB.

`tests/test_qemu_output.py` проверяет достоверность serial-доказательств: ошибка ранней загрузки не должна превращаться в PASS из-за последующего успешного маркера. Также проверяются отрицательный handoff, обязательный halt и различение начальных EL1/EL2. T-P1.2 добавляет `tests/test_exception.c` для причин/полей/first-entry guard, `test_exception_output.py` для UART-контракта, `test_fault_site.py` для связи symbol → исполняемые opcode-байты и `test_qemu_process.py` для жизненного цикла QEMU. В историческом T-P1.2 `host-python` дал **39/39 unittest PASS**, включая восемь проверок остановки процесса. Счётчик Python unittest отличается от количества этапов runner; актуальные числа T-P1.8A берутся из нового отчёта.

Исторический ARM ESR `0x96000061` host-декодер узнаёт как `alignment-fault` (Data Abort текущего EL). Это тест классификатора на сохранённом числе. Реальный Data Abort новым обработчиком в T-P1.2 не провоцировался и не объявляется проверенным.

`tests/test_boot_pool.c` проверяет внутренний пул: размер и весь полуинтервал, переполнение, выравнивание, полное резервирование образа, перекрытия с живыми областями, отказ до допуска, уникальность, исчерпание, неверные освобождения, сохранность состояния при отказе, повторную выдачу и полное обнуление. `tests/test_pool_site.py` повреждает ELF/PE-доказательства, включая случай, когда начало объекта допустимо, но конец уже за границей. `tests/test_pool_output.py` проверяет строгую связь UART с бинарными границами и отвергает ложный PASS.

### Сборка и форматы

```sh
python3 tools/build.py --arch x86_64 --output build
python3 tools/build.py --arch aarch64 --output build
```

x86 получается как EFI application PE32+ AMD64, без целевой libc, с entry UEFI. AArch64 получается как ELF для анализа и raw Image с ARM64 header для QEMU `virt`. Проверяются наличие/формат целевых артефактов и завершающие статусы инструментов.

Каждая сборка также создаёт `pool-site.json`. На ARM проверяются размеры символов, полный writable NOBITS/PT_LOAD, обе границы очистки BSS, `__image_end` и размер/содержимое raw Image. На x86 linker map даёт RVA, а PE подтверждает весь размер объектов внутри writable zero-fill секции и `SizeOfImage`. JSON связан SHA-256 с соответствующими артефактами. Он не заменяет последующее сравнение с runtime-адресами.

Один C-исходник не означает одинаковые бинарные байты для двух ISA. `build/manifest.json` описывает SHA-256 каждой архитектурной сборки отдельно.

### Повторяемость

`--repro` собирает одинаковые исходники одним toolchain в независимых выходных местах и сравнивает артефакты. Сборщик фиксирует PE timestamp и параметры GPT/FAT; linker ELF работает без случайного build ID. Сравнение должно охватывать EFI, диск, ARM ELF и raw Image.

Такой результат действует в пределах проверенного набора инструментов. Побайтная идентичность между разными версиями Clang/LLD не обещается. Изменение исходника или версии boot contract не должно скрываться переписыванием ожидаемого хеша.

### QEMU smoke и negative handoff

Обычная сборка должна достичь `kernel_main`, подтвердить `boot_info`, вывести `HANDOFF PASS`, карту, `MEMMAP PASS` и затем `STYLEOS KERNEL HALT`. Runner независимо вычисляет набор страниц из RAM и резервов и сверяет его с кандидатами, а также проверяет, что образ, стек, handoff и источник карты покрыты резервом. Ошибки boot/exception, неожиданная `STYLEOS EXCEPTION` или `STYLEOS EMERGENCY HALT` и маркеры неверной архитектуры означают неуспех прежнего normal/negative-сценария.

В отдельной negative-сборке `--invalid-boot-info` намеренно портит magic непосредственно перед общим ядром:

```sh
python3 tools/build.py --arch all --output build/negative --invalid-boot-info
```

Это тестовый вариант, его нельзя путать с обычной поставкой. Ожидается `STYLEOS BOOT_INFO ERROR: magic`, остановка и отсутствие `HANDOFF PASS`. То, что машина остановилась, само по себе не достаточно: тест должен различать ожидаемую диагностированную ошибку и потерю управления без маркеров.

В negative-сборке `--invalid-memory-map` handoff остаётся правильным, но на вход парсера попадает обрезанная карта. Для x86-64 ожидается `MEMMAP ERROR: uefi-format`, для ARM64 — `MEMMAP ERROR: fdt-format`. Частичные диапазоны и `MEMMAP PASS` запрещены. Оба отрицательных образа размещаются отдельно от обычного. ARM64 также запускается с 128 и 256 МиБ для проверки фактического чтения объёма RAM из DTB.

Ожидание halt — нормальное состояние прототипа. Автоматический тест ограничивает время процесса, собирает serial и прекращает VM. Timeout без всех ожидаемых маркеров не считается PASS. QEMU TCG проверяет исполняемый машинный путь; KVM и аппаратная виртуализация не являются требованием этого этапа.

### Реальные исключения T-P1.2

```sh
# Обе архитектуры, отдельный выход по умолчанию build/exception:
python3 tools/build.py --arch all --test-exception
# Тот же режим в явном отдельном каталоге:
python3 tools/build.py --arch all --test-exception --output build/exception-check
```

`--test-exception` взаимоисключающий с `--invalid-boot-info`, `--invalid-memory-map`, `--test-bootpool`, обоими paging-, обоими ownership-флагами и `--test-tasks`; обычный корневой `build` нельзя явно выбрать выходом exception-сборки. Trigger отсутствует в обычных образах. В тестовом образе собственные векторы/IDT уже установлены, handoff валиден и карта дошла до `MEMMAP PASS`; только затем исполняются `UD2` либо `BRK #0x120`.

Три реальные QEMU-проверки — `qemu-x86_64-exception`, `qemu-aarch64-exception`, `qemu-aarch64-exception-el2`. Каждая требует ровно одну полную строку `STYLEOS EXCEPTION arch=...`, затем отдельный `STYLEOS EMERGENCY HALT`, без `STYLEOS KERNEL HALT`. В строке ARM ожидаются EC `0x3c`, `reason=brk`, ESR/ELR/SPSR и `far=absent`; в строке x86 — vector 6, `reason=invalid-opcode`, `error=absent cr2=absent`. Служебный ноль нормализации кадра не считается аппаратным error code.

Сохранённый PC сверяется с `fault-site.json`, созданным из реального test executable и символа `styleos_test_fault`. ARM ELF и raw Image должны содержать opcode `002420d4` (`BRK #0x120`) на нужном месте. PE должен содержать `0f0b` (`UD2`) в исполняемой секции; runtime RIP равен **загруженному ImageBase + RVA**. Preferred ImageBase не подменяет фактический адрес загрузки.

В историческом Linux-прогоне T-P1.2 x86 saved RIP `0x602db42` совпал с runtime base `0x6029000` + RVA `0x4b42`; preferred base был `0x140000000`. ARM EL1 и EL2 → EL1 дали saved PC/ELR `0x40083908`, ESR `0xf2000120` и `far=absent`. Эти адреса относятся к конкретной сборке: после изменения кода runner вычисляет новый ожидаемый адрес из нового образа.

После полного терминального маркера harness ещё **200 мс** наблюдает процесс. Самостоятельный exit (включая код 0), reset, неполная строка, отсутствие маркеров или один timeout не проходят. Затем harness штатно останавливает живой QEMU; возврат 0 после запрошенного SIGTERM допустим. Состояние наблюдения и остановки записывается в журнал. Host-регрессии проверяют эти различия, чтобы успешная строка перед аварийным выходом не создавала ложный PASS.

Собственный обработчик работает только после своей точки установки; broken stack/UART, firmware GDT, неподдерживаемые асинхронные пути и поведение повторного сбоя имеют пределы [ADR-0005](adr/0005-early-exception-diagnostics.md). Результат BRK/UD2 не доказывает обработку всех EC/векторов или восстановление исполнения.

## Пул внутри образа T-P1.3

```sh
python3 tools/build.py --arch all --test-bootpool
# Отдельный каталог по умолчанию build/bootpool:
python3 tools/build.py --arch aarch64 --test-bootpool --output build/pool-check
```

`--test-bootpool` взаимоисключающий с двумя отрицательными флагами, `--test-exception`, обоими paging-, обоими ownership-флагами и `--test-tasks`. Обычный `build` нельзя явно выбрать выходом самотеста. Обычные образы содержат зарезервированный массив, но не выполняют тестовую выдачу, запись или чтение страниц.

После успешного `MEMMAP PASS` самотест допускает пул, печатает RANGE/META/WORKSPACE и требует следующий порядок:

```text
STYLEOS BOOTPOOL RANGE 0x… 0x…
STYLEOS BOOTPOOL META 0x… 0x…
STYLEOS BOOTPOOL WORKSPACE 0x… 0x…
STYLEOS BOOTPOOL ADMIT PASS
STYLEOS BOOTPOOL ALLOC 0 0x… ZERO
…
STYLEOS BOOTPOOL ALLOC 7 0x… ZERO
STYLEOS BOOTPOOL RW PASS pages=8 bytes=4096
STYLEOS BOOTPOOL EXHAUSTED PASS
STYLEOS BOOTPOOL REUSE 0x… ZERO
STYLEOS BOOTPOOL PASS
STYLEOS KERNEL HALT
```

Многоточия выше — схема для чтения, а не допустимый реальный журнал. Настоящие адреса имеют 16 шестнадцатеричных цифр, записи ALLOC идут по индексам 0–7, каждый адрес уникален и охватывает целую страницу внутри пула. Reuse должен вернуть именно ранее освобождённую страницу №3. Самотест читает/пишет все байты разных шаблонов, а после reuse проверяет обнуление и сохранность остальных семи страниц.

QEMU-сценарии — `qemu-x86_64-bootpool`, `qemu-aarch64-bootpool` и `qemu-aarch64-bootpool-el2`. Runner сопоставляет весь runtime RANGE/META с `pool-site.json`: на ARM — с ELF, на x86 — с реальным MEMMAP IMAGE start + RVA, без подстановки preferred ImageBase. Проверяются оба конца внутри IMAGE/RESERVED, отсутствие перекрытий со стеком, handoff, source, workspace и метаданными, точный набор выдаваемых страниц и порядок всех итогов. Память `MEMMAP CANDIDATE` не предоставляется пулу.

`BOOTPOOL ERROR`, exception/emergency marker, пустой журнал, timeout, отсутствующие/неподтверждённые границы, повторный адрес или PASS без выполненных шагов означают FAIL. Полный terminal marker дополнительно проходит прежнюю проверку живого процесса и штатной остановки harness. Самотест подтверждает фиксированный image-owned пул в выбранных QEMU-профилях, не общий физический аллокатор. Основания для сравнения адресных пространств и ограничения API — [ADR-0006](adr/0006-image-owned-boot-pool.md).

## Собственные таблицы T-P1.4

```sh
python3 tools/build.py --arch all --test-paging
# Отдельный build/paging.
python3 tools/build.py --arch all --test-paging-fault
# Отдельный build/paging-fault.
```

Все десять тестовых флагов взаимоисключающие, включая оба ownership-профиля T-P1.5, task-профиль T-P1.6 и user-processes-профиль T-P1.7. Новые образы нельзя явно записать в обычный корневой `build`. Paging-код, 32-страничный table reserve, control/data и fault site не включаются в обычные, старые, ownership-, task- или user-processes-варианты; positive paging image также не содержит fault site. Отдельные binary/source-list проверки подтверждают разделение профилей, а не просто разные имена каталогов.

Host-набор добавляет `test_paging.c` для построителя/реального descriptor lookup, `test_paging_admission.c` для полных резервов/исключений и `test_paging_x86.c` для чистой модели режима/унаследованного walker. Проверяются края, переполнение, PA-ширина, конфликт VA и alias rights/type, finite capacity, missing leaf/probe, неподходящий режим и `root=0` на отказе. `test_paging_site.py` повреждает PE/ELF/Image/initializer/fault proofs; `test_paging_output.py` проверяет строгий UART-контракт. Эти host fixtures не являются аппаратными запусками.

Новые сборки создают `paging-site.json`: полные tables/control/data объекты, отсутствие пересечения с boot pool, IMAGE/BSS/PE-section bounds, известный initializer, hashes. Fault-вариант дополнительно связывает `styleos_paging_fault` с настоящими байтами executable read. Runner заново выводит proof из актуальных бинарников, сверяет sidecar и SHA загрузочного raw/GPT-образа, затем учитывает runtime PE base. Preferred base не подставляется вместо фактической.

Схема порядка UART (многоточия — только объяснение, не принимаемый журнал):

```text
STYLEOS MEMMAP PASS
STYLEOS PAGING TABLES <start> <end>
STYLEOS PAGING CONTROL <start> <end>
STYLEOS PAGING DATA <start> <end>
STYLEOS PAGING WORKSPACE <start> <end>
STYLEOS PAGING <X86|ARM> BEFORE ...
# Для x86 также GDT и IDT.
STYLEOS PAGING MAP va=... pa=... bytes=... flags=...
...
STYLEOS PAGING ROOT pa=... pages=...
STYLEOS PAGING <X86|ARM> AFTER ...
STYLEOS PAGING PTE va=... pa=... flags=...
...
STYLEOS PAGING ALIAS va=... pa=... before=... after=...
```

ROOT должен быть первой страницей собственного полного резерва, число использованных таблиц — в его пределах. AFTER содержит фактический CR3/режим x86 или TTBR/TCR/MAIR/SCTLR/VBAR ARM. Runner сверяет точный ограниченный MAP-набор и **каждую** фактическую PTE после перехода: image closure RWX, data alias RW/NX, x86 GDT NX с RO либо узким RW-разрешением для Accessed либо ARM PL011 Device RW/NX. Flags используют WRITE=1, EXEC=2, DEVICE=4; ноль означает RO/NX normal, а не отсутствие страницы. Image closure намеренно RWX и не доказывает W^X. GDT record содержит исходный raw descriptor: runner независимо проверяет A и разрешает RW только странице его Accessed-байта при A=0, требуя полный PA-резерв вне CANDIDATE. Остальные GDT pages должны быть RO; программная запись дескриптора не выполняется.

Alias VA `0x0000400000000000` видит PA выделенной data page. Before обязан быть `0x123456789abcdef0`, after — `0xfedcba9876543210`; код читает через alias, пишет через alias и проверяет исходный адрес. Это отдельное свидетельство работы новой трансляции, а не один маркер записи регистра.

Положительные jobs `qemu-x86_64-paging`, `qemu-aarch64-paging` и `qemu-aarch64-paging-el2` требуют затем ровно `STYLEOS PAGING PASS` и `STYLEOS KERNEL HALT`.

Fault jobs имеют суффикс `paging-fault` / `paging-fault-el2`. После тех же readback/PTE/alias проверок требуются `STYLEOS PAGING FAULT READY va=... pc=...`, одна настоящая exception-запись и один `STYLEOS EMERGENCY HALT`. Цель `0x0000400000001000` — канонический VA внутри выбранного режима без mapping. Обычный halt, PAGING PASS и продолжение запрещены:

- x86: `vector=14 reason=page-fault`, аппаратный `error=0`, `cr2=probe`; RIP совпадает с `mov rax,[rcx]` (`488b01`) после реальной PE relocation;
- ARM: `class=0x25 reason=translation-fault`, read/translation DFSC и достоверный `far=probe`; PC/ELR совпадают с `ldr x1,[x0]` (`010040f9`) в ELF и raw Image.

Любая подмена границ, mapped probe, неправильные root/права/тип/PC/syndrome, пропущенная или лишняя запись, ERROR, timeout, reset и тишина означают FAIL. После терминального маркера сохраняются прежние 200 мс наблюдения живого QEMU и штатная остановка harness; собственный exit 0 до этой остановки не принимается. Старые UART-протоколы не меняются ради нового опыта и отвергают неожиданные PAGING-записи.

Подробные входные режимы и пределы доказательства — [ADR-0007](adr/0007-owned-translation-tables-experiment.md). Публикуемые UART/JSON должны соответствовать конкретным SHA и версии toolchain; новые результаты не выводятся из успешного T-P1.3.

## План защиты физической памяти T-P1.5

```sh
python3 tools/build.py --arch all --test-ownership-plan
# Отдельный build/ownership.
python3 tools/build.py --arch all --test-ownership-plan-invalid
# Отдельный build/ownership-invalid.
python3 tools/build.py --arch aarch64 --test-ownership-plan --output build/ownership-arm-check
```

Все девять флагов — `--invalid-boot-info`, `--invalid-memory-map`, `--test-exception`, `--test-bootpool`, `--test-paging`, `--test-paging-fault`, `--test-ownership-plan`, `--test-ownership-plan-invalid`, `--test-tasks` — попарно несовместимы. Оба ownership-варианта требуют отдельного выхода и не записываются в обычный корневой `build`. Их код и 65536-байтный BSS workspace отсутствуют в прежних образах; paging buffers/fault symbols в ownership-образе запрещены. Старые профили и четыре обычных артефакта проверяются прежними регрессиями и воспроизводимостью.

`host-ownership` запускает C-проверки `tests/test_ownership.c` и чистые helper-проверки `tests/test_ownership_x86.c`. Покрываются вложенные/пересекающиеся резервы, частичная страница, image tails, overflow, ёмкость, неподдерживаемый источник/тип, потеря обязательного живого диапазона, противоречивое/отсутствующее доказательство, проверка RESERVED до чтения и нулевая выдача. `test_ownership_site.py` проверяет полные PE/ELF/BSS/Image объекты и sidecar hashes. `test_ownership_output.py` отвергает подменённые границы, выход за известный набор RAM/резервов, неправильную фазу/root/translation, ложный PASS, ненулевой grant и частичную публикацию. Host fixtures не являются аппаратным запуском.

Новый `ownership-site.json` содержит `arch`, адресное пространство, полный `image_start/end/size`, ARM `bss_start/end` либо PE `preferred_image_base`, `invalid`, SHA и `objects`. Полные объекты: workspace 65536, diagnostic 12400, stack 65536, pool 32768, pool_metadata 4096 байт; дополнительно PE idt 4096 и handoff 96 либо ARM vectors 2048 байт. PE-координаты — RVA: runner прибавляет к обоим концам фактический MEMMAP IMAGE base. ARM-координаты — проверенные identity image addresses, без предположения о произвольном C-указателе. У IDT требуется только 16-byte alignment, поэтому её наружная защита может занять две страницы.

`verified_ownership_site()` заново выводит proof из текущих бинарников, сравнивает весь sidecar и SHA бинарника/карты/raw Image/GPT-диска и manifest, включая флаги режима. UART затем сверяется с этими полными объектами и текущим MEMMAP. Нельзя заменить доказательство найденной строкой PASS или preferred PE base.

Схема положительного UART; многоточия предназначены только для объяснения:

```text
STYLEOS MEMMAP PASS
STYLEOS OWNERSHIP PLAN arch=<x86_64|aarch64> phase=<inherited|mmu-off> source=<uefi|fdt>
STYLEOS OWNERSHIP WORKSPACE pa=... bytes=...
STYLEOS OWNERSHIP SOURCE pa=... bytes=... stride=... version=...
STYLEOS OWNERSHIP STATE mode=... root=... ...
STYLEOS OWNERSHIP CLAIM kind=... proof=... va=... pa=... bytes=...
...
STYLEOS OWNERSHIP WALK kind=<GDT|TABLE> va=... pa=... levels=...
STYLEOS OWNERSHIP STEP level=... table=... entry=...
...
STYLEOS OWNERSHIP INACTIVE paging=not-built pool=not-admitted
STYLEOS OWNERSHIP UNRESOLVED flags=...
STYLEOS OWNERSHIP PROTECTED <start> <end>
...
STYLEOS OWNERSHIP REVIEW <start> <end> reason=no-transfer
...
STYLEOS OWNERSHIP UNCLASSIFIED outside-covered-union
STYLEOS OWNERSHIP TOTAL protected=... review=... granted=0x0000000000000000
STYLEOS OWNERSHIP STATE AFTER mode=... root=... ...
STYLEOS OWNERSHIP PASS
STYLEOS KERNEL HALT
```

GDT/TABLE `WALK/STEP` есть только на x86. Runner воспроизводит все заявленные реальные пути и связывает их с claims и старым root; общие слоты должны согласовываться, аппаратные A/D могут отличаться. Это проверка GDT translations и identity посещённых table pages. Полный image identity проверяет adapter; UART не выдаётся за полный дамп старых image leaves или доказательство всех таблиц.

На x86 перед чтением table/GDT страницы требуется полный baseline RESERVED вне CANDIDATE, а физический доступ опирается на узкий UEFI/OVMF контракт. Обязательный `unresolved=3` сохраняет явную неполноту полного набора старых таблиц и прочих firmware-зависимостей. ARM при обоих начальных EL проверяет фактический EL1/MMU off/VBAR/DAIF и `root=0` для этой фазы; это не чтение или очистка TTBR0. PL011 — отдельный MMIO claim. Его `unresolved=0` также не передаёт REVIEW ядру.

Runner независимо объединяет baseline RESERVED и округлённые наружу живые PA claims, вычитает их из CANDIDATE и проверяет точный отсортированный PROTECTED/REVIEW и счётчики страниц без повторного учёта вложений. Каждый REVIEW имеет `reason=no-transfer`, `granted` равен нулю. Снимок AFTER фактически читается после построения **до первой публикации PLAN** и требует прежние значимые регистры: CR3/MMU/GDT и IRQ-состояние не меняются. Новый код пишет только свою metadata и существующий C-стек, общая RAM не пробуется, не выдаётся и не освобождается.

Invalid-профиль снимает IMAGE proof только в своём input, не портя handoff/map/firmware. После правильного MEMMAP требуется ровно `STYLEOS OWNERSHIP ERROR: required`; PLAN/CLAIM/PROTECTED/REVIEW/PASS, обычный KERNEL HALT и частичный план запрещены. Успех negative job означает проверенный отказ, а не построенный план.

Новые QEMU jobs: `qemu-x86_64-ownership`, `qemu-x86_64-ownership-invalid`; `qemu-aarch64-ownership`, `qemu-aarch64-ownership-el2`, `qemu-aarch64-ownership-256m`; `qemu-aarch64-ownership-invalid`, `qemu-aarch64-ownership-invalid-el2`. ARM 128/256 МиБ должны дать изменение диапазонов из реального DTB, а не из подставленного объёма QEMU. Старые шесть paging jobs остаются в наборе.

Для каждого нового job сохраняются `.log` и одноимённый `.json` в `test-results`. Положительный JSON содержит архитектуру, фазу/источник плана, claims, PROTECTED/REVIEW, причины, unresolved, нулевой grant, binary/manifest/UART SHA и сведения о процессе; negative содержит подтверждённую причину отказа без частичного плана. PASS публикуется только после полной UART/binary проверки и прежних 200 мс наблюдения живого QEMU. Естественный exit, reset, timeout или тишина не принимаются. Ограничения и владельцы каждого диапазона — [ADR-0008](adr/0008-physical-memory-ownership-plan.md).

## Две кооперативные задачи T-P1.6

```sh
python3 tools/build.py --arch all --test-tasks
# Отдельный build/tasks; также разрешён иной отдельный каталог:
python3 tools/build.py --arch aarch64 --test-tasks --output build/tasks-arm-check
python3 tools/test.py --all --output build/t-p1-6
```

`--test-tasks` несовместим со всеми восемью прежними test-флагами; проверяются все 36 пар девяти режимов. Явный вывод task-сборки в обычный корневой `build` запрещён. Task-код, `arch/*/tasks_context.S`, два стека и control union отсутствуют в ordinary и всех прежних профилях. Обратная проверка исключает paging/ownership/fault-объекты из task-профиля. Новая зависимость от LLVM objdump относится только к этому профилю.

После валидного handoff и успешной карты допускаются два непересекающихся 16384-байтных BSS-стека и 4096-байтный BSS control union, все с выравниванием 4096. У каждого стека по 16 байт software canary снизу и сверху; полезный полуинтервал — `[start+16,end-16)`. Диспетчер продолжает использовать прежний 65536-байтный стек ядра. До новых записей проверяются полные объекты, их концы, резервирование и отсутствие перекрытия с активным стеком, handoff, источником UEFI/DTB, diagnostic workspace, pool/metadata и архитектурными зависимостями. Это статическая память образа: пул не допускается и не выделяет страницы, CANDIDATE/REVIEW не читаются и не выдаются, смысл `GRANTED=0` сохраняется.

`host-tasks` запускает `tests/test_tasks.c` с `kernel/tasks.c`. Проверяются FREE → READY → RUNNING → READY/DONE, пустой диспетчер до создания и после завершения, два слота и отказ третьему, null entry, неверный размер/конец/выравнивание/overflow стека, перекрытия, неверные переходы/current, переполнение счётчиков, неизменность **всех байтов** control и output при отказе. DONE не выбирается и не переиспользуется до release; release очищает только context/metadata слота, не RAM стека. Тесты также проверяют сохранность другого слота и неизменность настоящего host-массива стека. Они не исполняют entry-функции и не доказывают аппаратное переключение.

`tasks-site.json` связывает полный PE/map либо ELF/BSS/raw Image с точными размерами stack_a/stack_b/control, прежним стеком, diagnostic, pool/metadata и IDT/handoff либо vectors. Для ARM проверяются NOBITS/PT_LOAD, VMA=LMA, оба BSS clear bounds и весь объявленный raw Image; для x86 — writable zero-fill extent и `SizeOfImage`. PE-числа — RVA, к которым runtime-проверка прибавляет **реальный** Loaded Image base. Чужой C-указатель не становится PA вследствие приведения типа.

В том же proof записаны реальные compiler flags, версия/команды LLVM decoder, границы и SHA исполняемых spans, полное число декодированных байтов и instructions. Декодер проверяет байты всего исполняемого содержимого связанного профиля, включая путь задач и переключения; неизвестная инструкция, FP/SIMD, разрыв или несовпадение байтов — отказ. В ARM header исполняется первая 4-байтная branch; байты `[Image+4,Image+64)` — объявленные header data и не декодируются как команды. Прежние флаги x86 `-mno-red-zone -mno-sse -mno-mmx -msoft-float` и ARM `-mgeneral-regs-only` остаются дополнительной проверяемой предпосылкой. Общий FP/SIMD context не реализован. `verified_tasks_site()` заново выводит proof из текущих файлов и сверяет sidecar, manifest и SHA загружаемых артефактов.

Машинные adapters сохраняют SP, PC и nonvolatile integer registers реальной ABI: x86 RBX/RBP/RDI/RSI/R12–R15; ARM X19–X30. Probe сначала сохраняет исходные регистры вызывающего C-кода на собственном стеке задачи, устанавливает разные шаблоны для двух yield-точек и выполняет прямой CALL/BL переключателя. После настоящего возврата к resume label ASM сравнивает реальные восстановленные регистры, затем возвращает C-коду исходные caller values. Полная маска — `0xff` для восьми x86-регистров и `0x7ff` для ARM X19–X29; ARM X30 проверяется через доказанное продолжение. Разбор бинарника связывает entry/resume/exit PC с исполняемыми границами инструкций. Начальные кадры, shadow space, return trap и пределы ABI разобраны в [ADR-0009](adr/0009-cooperative-kernel-tasks.md).

В каждом реальном запуске требуется один порядок событий:

```text
A:START → B:START → A:RESUME → B:RESUME → A:EXIT → B:EXIT
```

Это схема событий, а не допустимая замена полному UART. Журнал должен содержать допуск полных диапазонов, создание обеих задач, шесть GO, два yield и два успешных register-resume на задачу, возврат каждой entry-функции через bootstrap в DONE и штатное возвращение диспетчеру. Saved SP должен лежать в собственном полезном stack interval, saved PC — совпадать с конкретным продолжением из binary proof; печатаются реальные saved registers. У A и B разные адреса volatile local cookie в своих стеках и разные значения, пережившие оба yield. Адрес dispatcher cookie должен остаться в прежнем стеке ядра. Затем проверяются DONE, EMPTY без повторного выбора, release и canaries, неизменность архитектурного состояния, ровно один `STYLEOS TASKS PASS` и прежний `STYLEOS KERNEL HALT`.

Порядок task-записей после MEMMAP: `STACK id=A/B`, `CONTROL`, `MAP_WORKSPACE`, `DISPATCH_STACK`, `STATE`, два `CREATE state=READY`; затем для каждого GO идут `EVENT kind=START/RESUME/EXIT`, `YIELD` либо `RETURN`, `SAVED state=READY/DONE`, все `REG` после yield и `DISPATCH`. В конце — `EMPTY`, два `RELEASE state=FREE`, два `CANARY`, `CANARY PASS`, `STATE AFTER`, `TASKS PASS`, обычный halt. Все поля-числа имеют фиксированную форму `0x` и 16 hex digits. Tags двух yield: A `0x101/0x102`, B `0x201/0x202`. `EVENT regs` равен нулю только на START; RESUME и EXIT обязаны вернуть полную register mask. Saved task PC связан с probe/exit resume, а dispatcher PC — с `styleos_tasks_dispatch_resume` из связанного ASM wrapper.

Новые QEMU jobs — `qemu-x86_64-tasks`, `qemu-aarch64-tasks`, `qemu-aarch64-tasks-el2`. На x86 сохраняются inherited CR3/GDT/IDT/CS и выключенные IF/DF; ARM initial EL1 и EL2 → EL1 остаются в EL1h с MMU off, прежними VBAR/DAIF. Архитектурный observer только читает: x86 допускает table/GDT страницы по целому RESERVED/non-CANDIDATE extent, доказывает image identity и bounded closure посещённых таблиц. Это узкий UEFI/OVMF контракт, а не общий VA→PA service или полный перечень firmware state. На ARM известный Image доказывается linker/header и фактическим MMU-off состоянием. Регистры трансляции и exception-таблицы не меняются.

UART parser отвергает PASS-only, пропущенное/повторное resume, неверный порядок, SP вне своего стека, изменённый cookie/register pattern, несовпадающий PC и повторный вход после exit. Для каждого job нужны UART `.log`, проверенный `.json`, `tasks-site.json` и manifest, связанные SHA. Прежнее наблюдение живого QEMU в течение 200 мс после terminal marker и штатное прекращение harness сохраняются: timeout, reset, естественный exit или тишина не означают PASS. Итоговое число выполненных проверок и статусы берутся только из [отчёта T-P1.6](test-results/t-p1-6/README.md).

Регрессия включает прежние сценарии, воспроизводимость четырёх ordinary artifacts и сравнение 28 прежних profile artifacts с базой при том же toolchain. Обновлять ожидаемые хеши вместо разбора различия нельзя. Результат T-P1.6 доказывает смену стека, продолжения и указанного integer-состояния доверенных функций. Software canaries не останавливают overflow и не создают изоляцию. Нет user mode, защиты задач друг от друга, recovery, IRQ/таймера, вытеснения, SMP, heap, syscalls, IPC или Deep Drop. Сам результат T-P1.6 не доказывает сдерживание сбоя пользовательского процесса; для этого нужен отдельный профиль T-P1.7.

## Пользовательские программы и сдерживание fault T-P1.7

```sh
python3 tools/build.py --arch all --test-user-processes
# Отдельный build/user-processes; иной отдельный выход разрешён явно:
python3 tools/build.py --arch aarch64 --test-user-processes --output build/user-arm64
```

Новый флаг несовместим со всеми девятью прежними режимами. Только этот профиль включает CPL3/EL0-программы, три отдельных корня SAFE/A/B, конечные image-owned страницы и собственный аппаратный вход. Старые профили сохраняют свои источники, таблицы, векторы, GDT/IDT и артефакты; обычная загрузка не получает user-код. Разрешение новой GDT/TSS на x86 и MMU/user vectors на ARM не меняет исторические правила других профилей.

После handoff/MEMMAP проверяются полные интервалы таблиц, control, привилегированного стека входа, двух пользовательских стеков, data и code pages, а также x86 GDT/TSS/IDT. Требуются целый RESERVED image extent, правильные выравнивание/размеры/арифметика и непересечение других живых объектов. Общая RAM, CANDIDATE/REVIEW, boot pool, PMM/heap и reclaim не используются. Корни A/B имеют ровно свои user code RX, stack RW/NX и data RW/NX; SAFE user leaves не имеет. Kernel image aliases доступны только привилегированному режиму, включая таблицы и metadata. Отдельно доказываются отсутствие чужих приватных отображений и отсутствие user-алиаса, обходящего права.

`user-processes-site.json` и `user-processes-compiler.json` связаны с manifest и фактическими SHA конечных PE/ELF/Image/диска. Проверяются весь extent каждого объекта, code pages и точные offset/opcode тестового чтения и переходов. Пользовательский fault PC равен фиксированному code VA плюс offset инструкции внутри встроенной программы; физический адрес символа в PE после relocation — отдельное число. Приведение произвольного C-указателя не является переводом в PA. Полное integer-only декодирование и реальные compiler flags ограничивают область сохраняемого контекста; общей FP/SIMD-смены нет.

Host lifecycle проверяет READY/RUNNING/FAULTED/EXITED, порядок A yield → B yield → A fault → B exit, запрет повторного выбора терминального A и release только после аппаратно подтверждённого SAFE-root. Отказы по argument/state/overlap/capacity/overflow не должны частично изменять живые слоты. Отдельные бинарные и parser-тесты повреждают происхождение exception, PC/VA, слот fault, права/таблицы, порядок завершения и continuation. Host-код не выполняет IRETQ/ERET и не доказывает user mode.

В трёх новых QEMU-сценариях требуется аппаратное доказательство:

1. A входит в CPL3/EL0 на своём стеке и уступает через INT/SVC. Аппаратный frame содержит user CS/SS либо SPSR EL0t, PC/SP, все integer registers и фактический active root.
2. B на другом root записывает `0x123456789abcdef0` в собственную data page и уступает. Запоминаются его продолжение, private data и stack value.
3. A возобновляется и выполняет известное чтение `0x12010000` — B-private VA, отсутствующего под root A. На x86 требуется #PF vector 14, hardware error `0x4`, CPL3, CR2 и точный RIP. На ARM — lower-EL Data Abort EC `0x24`, EL0t, read translation DFSC `0x7`, валидный FAR и точный ELR.
4. Только A становится FAULTED; аппаратный adapter возвращается под SAFE-root на исходный dispatcher stack. Аварийный PC не выбирается вновь.
5. B возобновляется **после fault**, сохраняет своё состояние, проверяет прежние данные, меняет их на `0xfedcba9876543210` и штатно выходит через аппаратный вход.
6. Под SAFE-root ядро исключает терминальные контексты из употребления, освобождает только учёт, проверяет dispatcher cookie/данные/порядок и лишь затем выводит итоговый PASS и штатный KERNEL HALT. Физические страницы остаются в image reserve.

Нужны полные UART/JSON и manifest/sites каждого x86/ARM initial EL1/ARM initial EL2→EL1 запуска. Проверяется идентичность действительных roots и полного набора user mappings, а не только числа, напечатанные до активации. Runtime сканирует конечные резервы таблиц и публикует все ненулевые дескрипторы; runner независимо восстанавливает дерево, эффективные права и точный набор leaves, отклоняя лишний user alias, чужую приватную PA или неизвестную table page. Привилегированный стек входа не зависит от user SP. Исключение ядра, #GP вместо ожидаемого #PF, другой VA/PC, fault не A, завершение B до fault или повторный выбор A — FAIL. Одна строка PASS, reset, timeout, тишина или естественный exit QEMU недостаточны. Сохраняется наблюдение живого эмулятора после терминального маркера и намеренная остановка harness.

База повторяется тем же toolchain **до** правок. Сравниваются все её профили, включая T-P1.6 tasks, и четыре обычных reproducibility артефакта; фактическое число публикуется в [отчёте T-P1.7](test-results/t-p1-7/README.md), старое число 36 не переносится автоматически. Проверенные и непроверенные части сдерживания, аппаратные frames и GDT/TSS/vectors разобраны в [ADR-0010](adr/0010-user-process-fault-isolation.md).

## Первый сохраняемый файл T-P1.8A

Новый storage-профиль работает только на AArch64 QEMU `virt` с одним CPU и
современным VirtIO Block MMIO. Загрузочный Image и диск данных раздельны.
Guest сам создаёт `note.txt`, metadata, payload и commit; host создаёт только
пустой обычный файл диска и управляет независимыми процессами VM.
Формат и пределы — [ADR-0011](adr/0011-persistent-test-file-storage.md).

Отдельная сборка для изучения исходников:

```sh
python3 tools/build.py --arch aarch64 --test-storage --output build/storage
```

Полный контрольный storage-прогон из корня закреплённого SHA — **одна команда**:

```sh
CC=clang python tools/test_storage.py --output build/termux-tp18a
```

Она сама выполняет host-проверки, собирает отдельный ARM boot image, создаёт два
пустых тестовых диска, запускает семь независимых процессов QEMU без графики
и сохраняет результаты. Ручное создание образа,
форматирование, выбор фаз или перенос файлов между VM не требуются.
Нет snapshot/restore: после завершения одного QEMU новый процесс открывает тот
же файл данных. Нельзя подставить пользовательский диск как готовый источник.

Каждый повтор создаёт новый `run-<UTC>-<random>` под указанным output.
Диски `persist.img` и `crash.img` создаются исключительно внутри его `disks/`
как новые regular files. Предыдущие run-каталоги, диски и свидетельства
сохраняются. Symlink/non-directory output и ранее существующий файл вместо
нового диска отвергаются; тест не открывает block devices хоста.

| Цепочка VM | Что проверяется |
|---|---|
| `persist-init-a → persist-read-a` | Guest фиксирует A и подтверждает final FLUSH, первый QEMU полностью прекращает работу; другой процесс читает точные 15 байт A |
| `persist-update-b → persist-read-b` | Guest фиксирует отличающиеся 24 байта B; следующий процесс читает именно B |
| `crash-init-a → crash-prepare-b → crash-recover-a` | На отдельном диске есть A; старый commit неактивной области обнулён с FLUSH, затем B записан с data FLUSH; новая commit record ещё не записана, guest неподвижен на барьере, runner делает SIGKILL, новый процесс читает целую A |

Каждый save выполняет три упорядоченных FLUSH: после обнуления commit sector
неактивной области, после metadata/payload и после новой commit record.
Предварительное обнуление с FLUSH исключает принятие оставшейся commit record
при повторной подготовке того же поколения после fallback к старой версии.
Явная инициализация дополнительно проверяет все 2048 нулевых секторов и
подтверждает superblock отдельным FLUSH.

Строгий UART parser требует следующие суммарные успешные операции за boot;
READ/WRITE здесь означают один 512-байтный сектор, FLUSH — отдельный запрос:

| Фаза | READ | WRITE | FLUSH | Всего запросов |
|---|---:|---:|---:|---:|
| `init-a` | 2053 | 6 | 4 | 2063 |
| `read-a`, `read-b`, `recover-a` | 14 | 0 | 0 | 14 |
| `update-b` | 19 | 5 | 3 | 27 |
| `prepare-b` | 14 | 4 | 2 | 20 |

Имя — `note.txt`, данные A — `StyleOS note A\n`, B —
`StyleOS note B: updated\n`; завершающий LF входит в длину. Host не передаёт
payload в guest и не исправляет файл диска между фазами. Guest проверяет имя,
generation, длину и каждый байт, а runner связывает UART/JSON с SHA boot image,
manifest, бинарным evidence и диска до/после. Read-only фазы не меняют SHA диска.

Обычный PASS требует полного guest-контракта, штатного halt, 200-мс observation
window и намеренной остановки VM через SIGTERM. Для прерывания разрешён только
точный pre-commit barrier после обоих подготовительных FLUSH: guest уже не
выполняет запись новой commit record и
не выдаёт итоговый PASS. Timeout, reset, ошибка и одиночная строка PASS не
засчитываются как ожидаемое отключение. После подтверждённого барьера и ещё
200 мс без новых событий runner делает SIGKILL и требует return code -9.
Прекращение QEMU не является проверкой
отключения питания телефона, host или физического накопителя.

Сохраняйте весь новый run-каталог: `run.json`, `provenance.json`,
`test-results/results.json`, поэтапные `.log`, `.json`, `.uart`,
`boot/manifest.json`, `boot/aarch64/storage-compiler.json` и
`boot/aarch64/storage-site.json`. Для передачи
свидетельств нужны точный `git rev-parse HEAD`, версии Python/Clang/LLD/QEMU,
данные этапов и хеши. Бинарные boot image, data disks и firmware в Git не
коммитятся.

Host-модели формата отдельно проверяют signatures, bounds/length/checksum,
fallback к годной старой версии, повторную подготовку после fallback с
инвалидизацией старой commit record и отказ при отсутствии годной версии.
Transport model отдельно имитирует negotiation, capacity/bounds, timeout и
status errors. Это не настоящие отказы устройства. Реальные QEMU-прогоны
подтверждают только фактически выполненные обмены и перезапуски VM.

Фактические счётчики нового storage runner, полного regression runner,
ARM64-only, Python tests и число сравниваемых базовых артефактов берутся из
[отчёта T-P1.8A](test-results/t-p1-8a/README.md), а не из прежнего счётчика.
Обычная воспроизводимость и все старые профили проверяются отдельно:

```sh
python3 tools/test.py --all --output build/t-p1-8a-regressions
OVMF_CODE=/nonexistent/styleos-code.fd OVMF_VARS=/nonexistent/styleos-vars.fd \
LLD_LINK=/nonexistent/lld-link QEMU_X86=/nonexistent/qemu-system-x86_64 \
CC=clang python3 tools/test.py --host --qemu --qemu-arch aarch64 --output build/arm-only-tp18a
```

Этот полный прежний набор по-прежнему требует LLVM objdump для task/user-processes
proof, а storage runner обходится обычными Clang/LLD, Python и ARM QEMU.

Для нового каталога **Android/Termux**, после установки инструментов:

```sh
pkg install git python clang lld qemu-system-aarch64-headless
git clone --branch feature/styleos-t-p1-8a --single-branch https://github.com/LPYURA2/OS.git styleos-tp18a
cd styleos-tp18a
git checkout --detach 6dfb61d49835c50d561ca2b4a509563e3c744e59
git rev-parse HEAD
python --version
clang --version
ld.lld --version
qemu-system-aarch64 --version
CC=clang python tools/test_storage.py --output build/termux-tp18a
echo $?
```

| Среда | Статус T-P1.8A | Граница |
|---|---|---|
| Linux, AArch64 QEMU virt/VirtIO MMIO | См. [фактический отчёт](test-results/t-p1-8a/README.md) | Реальные guest READ/WRITE/FLUSH и новые процессы с тем же файлом диска |
| Android/Termux владельца | SKIPPED до контрольного прогона нового SHA | Не наследует подтверждение T-P1.7 |
| VirtualBox, физическое оборудование | SKIPPED | Нового x86-драйвера и native disk испытаний нет |

После публикации T-P1.7 владелец подтвердил **Android/Termux 43/43, exit 0,
QEMU 11.1.3**, SHA `af898029ff71892d8e3c6a3308263624af722130`, оба ARM
user-processes сценария PASS. Источник — три скриншота, сырые UART/JSON не
приложены. Это отдельное owner-reported дополнение о предыдущем этапе;
исторические отчёты/checksums не меняются, T-P1.8A Android PASS не присваивается.

## Ручной запуск x86-64

Сначала соберите `build/x86_64/styleos.img`. Пример с подходящими OVMF CODE/VARS:

```sh
cp /usr/share/OVMF/OVMF_VARS_4M.fd build/OVMF_VARS-test.fd
qemu-system-x86_64 -machine q35 -accel tcg -m 128M -vga none \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=build/OVMF_VARS-test.fd \
  -drive if=none,id=styleos,format=raw,readonly=on,file=build/x86_64/styleos.img \
  -device virtio-blk-pci,drive=styleos \
  -display none -monitor none -serial stdio -net none -no-reboot
```

В serial после успешной загрузки ожидаются:

```text
STYLEOS x86_64 UEFI entry
STYLEOS x86_64 acquiring memory map
STYLEOS x86_64 ExitBootServices OK
STYLEOS x86_64 IDT ready cs=0x0000000000000038
STYLEOS kernel_main entered
STYLEOS boot_info validated v1
STYLEOS HANDOFF PASS x86_64
STYLEOS MEMMAP DIAGNOSTIC ONLY
…
STYLEOS MEMMAP PASS
STYLEOS KERNEL HALT
```

Значение CS выше относится к проверенному OVMF; адаптер читает активный CS, а не предполагает `0x38` на любой firmware. После последней строки завершите процесс QEMU. В графическом окне нет рабочего стола StyleOS: ранний вывод предназначен для COM1.

## Ручной запуск AArch64

```sh
qemu-system-aarch64 -machine virt -cpu cortex-a57 -m 128M \
  -accel tcg -net none -nographic -kernel build/aarch64/styleos.bin
```

Проверьте entry EL1, сообщение о DTB header, вход общего ядра, `STYLEOS HANDOFF PASS aarch64`, `MEMMAP PASS` и halt. Для выхода из этого терминального режима используйте `Ctrl+A`, затем `X`.

Результат обычного EL1 пути не доказывает EL2 путь. Если он проверяется отдельно, тест должен явно задать соответствующую конфигурацию QEMU и сохранить её параметры в журнале. Unsupported EL и ошибка DTB не считаются успешным boot.

## Матрица сред и честные статусы

- **PASS:** команда действительно выполнена и весь контракт проверен по фактическим файлам и журналу.
- **FAIL:** выполненная проверка выявила несоответствие; причина и журнал сохраняются.
- **SKIPPED:** проверка не выполнена; отсутствующий инструмент, доступ или устройство указаны явно.

Числа T-P1.7, версии, команды, базовый/проверенный/отчётный SHA и журналы находятся в [отчёте](test-results/t-p1-7/README.md). Число этапов считают по `results.json`, Python unittest — по `host-python.log`. Ожидаемый счётчик не является фактическим результатом.

| Среда | Статус T-P1.7 | Что может подтвердить |
|---|---|---|
| Linux, QEMU/OVMF x86-64 и ARM64 initial EL1/EL2 | См. [фактический отчёт](test-results/t-p1-7/README.md) | Аппаратный user frame, отдельные roots/стеки, ожидаемый fault A и последующий выход B |
| Linux, ARM64-only без OVMF/x86 tools | См. [фактический отчёт](test-results/t-p1-7/README.md) | Два новых ARM user-processes сценария и прежние ARM регрессии; это не Android |
| Android/Termux владельца | PASS, owner-reported после публикации | T-P1.7: 43/43, exit 0, QEMU 11.1.3, SHA `af898029ff71892d8e3c6a3308263624af722130`; три скриншота, без сырых UART/JSON |
| VirtualBox | SKIPPED до отдельного испытания | Linux QEMU не подтверждает эту платформу |
| Физическое оборудование | SKIPPED до отдельного испытания | QEMU virt не является нативной аппаратурой телефона |

База T-P1.7 — `eb73fad3ed431e91ebe995be4b370c678210e2b8`; проверенный код T-P1.6 — `d8065f31ae292eccd2558ddc8dc2cd91b78b855f`. В задании T-P1.7 владелец сообщил о **T-P1.6 Android/Termux: 39/39 PASS, код 0**, по скриншотам, оба ARM task-сценария PASS. Предварительная проверка `llvm-objdump` показала **21.1.8**; версия QEMU в этих скриншотах не показана. Сырые UART/JSON не приложены. Это отдельное owner-reported подтверждение T-P1.6; новый отчёт учитывает его без изменения исторических отчётов/checksums и без присвоения Android PASS более позднему этапу. После публикации владелец отдельно подтвердил **T-P1.7 Android/Termux: 43/43, exit 0, QEMU 11.1.3**, SHA `af898029ff71892d8e3c6a3308263624af722130`, оба ARM user-processes сценария PASS, по трём скриншотам; сырые UART/JSON не приложены. Это отдельное owner-reported дополнение, а не новый результат T-P1.8A.

Для Linux-проверки независимости ARM64-only намеренно задайте отсутствующие x86-инструменты и OVMF:

```sh
OVMF_CODE=/nonexistent/styleos-code.fd OVMF_VARS=/nonexistent/styleos-vars.fd \
LLD_LINK=/nonexistent/lld-link QEMU_X86=/nonexistent/qemu-system-x86_64 \
CC=clang python3 tools/test.py --host --qemu --qemu-arch aarch64 --output build/arm-only-tp17
```

Этот маршрут не должен обращаться к выбранным отсутствующим средствам. Сохранённый контрольный маршрут **T-P1.7** для отдельного каталога Termux:

```sh
pkg install python clang lld llvm git qemu-system-aarch64-headless
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

SHA команды закрепляется на опубликованной реализации из [отчёта T-P1.7](test-results/t-p1-7/README.md). `llvm-objdump` можно выбрать через `LLVM_OBJDUMP`. Runner сам останавливает QEMU; Ctrl+A не нужен. Сохраните все `build/termux-tp17/test-results/*.log` и `*.json`, особенно host lifecycle/Python, build user-processes и оба ARM user-processes UART/JSON; также соответствующие `manifest.json`, binary sites/compiler flags, старые task/paging/ownership sites и версии инструментов. Бинарные образы и firmware в текстовый отчёт не добавляются. ARM64-only не запускает x86 или reproducibility.

## Риски и откат

T-P1.8A публикуется только в `feature/styleos-t-p1-8a` от базы `2831e6a1c54562d1f37518913f07e648cc00cc2d`. `main`, старые refs, корневой README, `docs/styleos-readme`, Portable-os и исторические свидетельства сохраняются. Нет merge, force push или release. Только отдельный storage-профиль разрешает новый VirtIO MMIO драйвер и запись на созданный тестовый диск; старые профили этих разрешений не получают.

Сдерживание одного проверенного пользовательского fault не означает восстановления после ошибки ядра, полной безопасности, стабильного process ABI или поддержки произвольных программ. Нет общего PMM/heap, выдачи CANDIDATE/REVIEW, reclaim, IRQ/таймера/вытеснения/SMP, общего FP/SIMD-контекста, IPC, GUI или Deep Drop. T-P1.8A добавляет только минимальное тестовое хранение. T-P1.8B, внешние программы, desktop/SDK, `.tea` и Deep Drop этим заданием не начинаются.

Неподдержанная firmware, ошибка самого ядра, повреждённые kernel stack/table/UART либо отказ до готовности диагностического пути могут остановить boot. Неожиданное исключение ядра остаётся аварийным исходом; оно не засчитывается как успешное завершение A. Тесты используют выходные образы и отдельные копии firmware VARS, без личных дисков компьютера.

Для исследования прошлого состояния создаётся отдельная копия согласованного SHA; опубликованный откат выполняется новым revert после решения владельца. История не переписывается. После отчёта и публикации ветки работа останавливается.
