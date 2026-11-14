# Проверка StyleOS T-P1.4

Проверка разделяет четыре разных доказательства: C-логика на хосте, сборка целевых машинных форматов, побайтная повторяемость и прохождение загрузки в QEMU. PASS одного уровня не заменяет остальные. [Фактический прогон T-P1.4 и журналы](test-results/t-p1-4/README.md) публикуются отдельно; [T-P1.3](test-results/t-p1-3/README.md), [T-P1.2](test-results/t-p1-2/README.md) и прежние этапы сохраняются как история базы. Ниже описан воспроизводимый протокол текущего этапа.

## Требования и выбор инструментов

Нужны Python 3, Clang, `lld-link`, `ld.lld`; для target smoke — `qemu-system-x86_64`, `qemu-system-aarch64` и внешняя firmware OVMF x86-64. Make необязателен. Пакетирование GPT/FAT16 выполняется Python standard library, без `mtools` и монтирования образа.

В Debian/Ubuntu инструменты предоставляют `python3 clang lld make qemu-system-x86 qemu-system-arm ovmf`. Обычная установка выполняется владельцем или администратором окружения; сама сборка StyleOS не запрашивает root и не загружает пакеты.

Возможны пользовательские toolchain-каталоги. Выбор программ и firmware задаётся переменными:

| Переменная | Назначение |
|---|---|
| `CLANG` | host-исполняемый Clang с нужными target backends |
| `CC` | C-компилятор host-тестов, по умолчанию `cc` |
| `LLD_LINK` | LLD PE/COFF linker |
| `LD_LLD` | LLD ELF linker |
| `QEMU_X86` | `qemu-system-x86_64` |
| `QEMU_AARCH64` | `qemu-system-aarch64` |
| `OVMF_CODE` | read-only OVMF CODE firmware для x86-64 |
| `OVMF_VARS` | соответствующий исходный шаблон VARS |

CODE и VARS должны подходить друг к другу по варианту firmware. Шаблон VARS не должен быть личным изменяемым NVRAM-диском владельца; для теста используется отдельная копия. OVMF в Git не хранится.

Пример выбора установленных 4-МиБ OVMF файлов (пути проверьте на своём дистрибутиве):

```sh
export OVMF_CODE=/usr/share/OVMF/OVMF_CODE_4M.fd
export OVMF_VARS=/usr/share/OVMF/OVMF_VARS_4M.fd
python3 tools/test.py --all --output build/t-p1-4
```

## Точные команды

Из корня репозитория:

```sh
python3 tools/build.py --arch all --output build
python3 tools/test.py --host --output build
python3 tools/test.py --repro --output build
python3 tools/test.py --qemu --output build
# Либо один полный прогон:
python3 tools/test.py --all --output build/t-p1-4
```

Код завершения test runner: **0** — все выбранные проверки прошли; **1** — есть FAIL; **2** — есть SKIPPED без FAIL. Отсутствующий обязательный QEMU/OVMF виден в сводке и не превращается в зелёный прогон. Если флаг набора не задан, запускается полный набор.

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

`tests/test_qemu_output.py` проверяет достоверность serial-доказательств: ошибка ранней загрузки не должна превращаться в PASS из-за последующего успешного маркера. Также проверяются отрицательный handoff, обязательный halt и различение начальных EL1/EL2. T-P1.2 добавляет `tests/test_exception.c` для причин/полей/first-entry guard, `test_exception_output.py` для UART-контракта, `test_fault_site.py` для связи symbol → исполняемые opcode-байты и `test_qemu_process.py` для жизненного цикла QEMU. В историческом T-P1.2 `host-python` дал **39/39 unittest PASS**, включая восемь проверок остановки процесса. Счётчик Python unittest отличается от количества этапов runner; актуальные числа T-P1.4 берутся из нового отчёта.

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

`--test-exception` взаимоисключающий с `--invalid-boot-info`, `--invalid-memory-map`, `--test-bootpool` и обоими paging-флагами; обычный корневой `build` нельзя явно выбрать выходом exception-сборки. Trigger отсутствует в обычных образах. В тестовом образе собственные векторы/IDT уже установлены, handoff валиден и карта дошла до `MEMMAP PASS`; только затем исполняются `UD2` либо `BRK #0x120`.

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

`--test-bootpool` взаимоисключающий с двумя отрицательными флагами, `--test-exception` и обоими paging-флагами. Обычный `build` нельзя явно выбрать выходом самотеста. Обычные образы содержат зарезервированный массив, но не выполняют тестовую выдачу, запись или чтение страниц.

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

Все шесть тестовых флагов взаимоисключающие. Новые образы нельзя явно записать в обычный корневой `build`. Paging-код, 32-страничный table reserve, control/data и fault site не включаются в обычные или старые варианты; positive paging image также не содержит fault site. Отдельные binary/source-list проверки подтверждают изоляцию, а не просто разные имена каталогов.

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

- **PASS:** команда фактически выполнена, завершилась согласно контракту, нужные проверки/маркеры присутствуют.
- **FAIL:** команда выполнена и выявила несоответствие; причина и журнал сохраняются.
- **SKIPPED:** команда/платформа не проверена; указывается отсутствующий инструмент, доступ или устройство. SKIPPED не повышается до PASS по аналогии.

Фактические числа T-P1.4, команды, версии, SHA и журналы находятся в [отчёте](test-results/t-p1-4/README.md). Количество этапов считают по `results.json`, Python unittest — по `host-python.log`. Ожидаемый счётчик не является результатом.

Полный набор сохраняет normal/negative handoff/negative map, реальные UD2/BRK с PC/opcode proof, boot pool, ARM initial EL1/EL2, 128/256 МиБ и четыре обычных reproducibility artifacts. К ним добавлены host/binary/UART проверки и шесть новых QEMU paging jobs.

| Среда | Статус T-P1.4 | Граница результата |
|---|---|---|
| Linux, QEMU/OVMF x86-64 и AArch64 | См. [фактический отчёт](test-results/t-p1-4/README.md) | Новый positive/fault опыт и все прежние сценарии |
| Linux, ARM64-only без OVMF/x86 tools | См. [фактический отчёт](test-results/t-p1-4/README.md) | Включает обычные и оба новых ARM64 варианта; не телефон |
| Новый Android/Termux владельца | SKIPPED до фактического прогона | Не наследует телефонный PASS T-P1.3 |
| VirtualBox, физические устройства | SKIPPED до отдельного испытания | Ручной VirtualBox T-P1.3 не блокирует QEMU-приёмку T-P1.4 |

Принятая база `877f5e2bb85d484852a627e3a76a4bb956b40d50` содержит проверенный код пула `f10ee3313b7d8d428a3207e5bcfff6b93842be09` и последующие документы/журналы. Исторический T-P1.3: Linux 28/28 этапов, Python 71/71, ARM64-only Linux 20/20 и отдельный [Android/Termux владельца](test-results/t-p1-3/termux/README.md) 20/20 на QEMU 11.1.3. [T-P1.2](test-results/t-p1-2/README.md), [T-P1.1](test-results/t-p1-1/README.md) и [T-P1.0](test-results/t-p1-0/README.md) остаются отдельной историей. Ни один прежний результат не подтверждает новый paging-профиль.

Для проверки ARM64-only маршрута в Linux намеренно задайте отсутствующие x86-инструменты и OVMF:

```sh
OVMF_CODE=/nonexistent/styleos-code.fd OVMF_VARS=/nonexistent/styleos-vars.fd \
LLD_LINK=/nonexistent/lld-link QEMU_X86=/nonexistent/qemu-system-x86_64 \
CC=clang python3 tools/test.py --host --qemu --qemu-arch aarch64 --output build/arm-only-tp14
```

Этот эксперимент должен завершаться без обращения к выбранным отсутствующим средствам. Он остаётся Linux-проверкой.

Для нового Termux-прогона используйте новую папку и точный опубликованный SHA:

```sh
pkg install python clang lld git qemu-system-aarch64-headless
git clone --branch feature/styleos-t-p1-4 --single-branch https://github.com/LPYURA2/OS.git styleos-tp14
cd styleos-tp14
git checkout --detach 68314ee6be30096e70267e6b003fd4cc771dba0e
git rev-parse HEAD
qemu-system-aarch64 --version
CC=clang python tools/test.py --host --qemu --qemu-arch aarch64 --output build/termux-tp14
```

Runner сам завершает QEMU и не требует Ctrl+A. Сводку получите командой:

```sh
cat build/termux-tp14/test-results/results.json
```

Сохраните все `build/termux-tp14/test-results/*.log`, особенно `host-paging.log`, `build-paging.log`, `build-paging-fault.log`, `qemu-aarch64-paging.log`, `qemu-aarch64-paging-el2.log`, `qemu-aarch64-paging-fault.log`, `qemu-aarch64-paging-fault-el2.log`, обе pool- и обе exception-проверки. Нужны также `paging-site.json` из `paging/aarch64` и `paging-fault/aarch64`, manifest соответствующих сборок, точный `git rev-parse HEAD` и версии Python/Clang/LLD/QEMU. Этот маршрут не запускает x86 и reproducibility; до получения фактических файлов результат телефона остаётся SKIPPED.

## Риски и откат

Работа T-P1.4 остаётся в `feature/styleos-t-p1-4` от принятой базы `877f5e2bb85d484852a627e3a76a4bb956b40d50`. Обновление `main`, прежних refs, merge, force push, release и изменения Portable-os запрещены этой задачей.

Ошибки UEFI handoff, неправильный стек, неверные physical pointers и неподходящая firmware могут остановить boot до диагностики. Отсутствующий UART может скрыть результат, даже если CPU выполняется. Тесты должны фиксировать конфигурацию и отделять отсутствие вывода от ожидаемого halt.

Проверки работают с выходными образами и временными копиями firmware VARS. Личные диски/разделы компьютера не нужны. Для исследования старого состояния создаётся отдельная ветка/копия из согласованного коммита; откат опубликованного изменения выполняется новым `git revert` после решения владельца. История `main` не переписывается. Старый проект доступен по исходному SHA, архивной метке и в независимом Portable-os.
