# Проверка StyleOS T-P1.2

Проверка разделяет четыре разных доказательства: C-логика на хосте, сборка целевых машинных форматов, побайтная повторяемость и прохождение загрузки в QEMU. PASS одного уровня не заменяет остальные. [Фактический прогон T-P1.2 и журналы](test-results/t-p1-2/README.md) сохраняются отдельно; прежние [T-P1.1](test-results/t-p1-1/README.md) остаются историей базы; ниже описан воспроизводимый протокол.

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
python3 tools/test.py --all --output build/t-p1-2
```

## Точные команды

Из корня репозитория:

```sh
python3 tools/build.py --arch all --output build
python3 tools/test.py --host --output build
python3 tools/test.py --repro --output build
python3 tools/test.py --qemu --output build
# Либо один полный прогон:
python3 tools/test.py --all --output build/t-p1-2
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

`tests/test_qemu_output.py` проверяет достоверность serial-доказательств: ошибка ранней загрузки не должна превращаться в PASS из-за последующего успешного маркера. Также проверяются отрицательный handoff, обязательный halt и различение начальных EL1/EL2. T-P1.2 добавляет `tests/test_exception.c` для причин/полей/first-entry guard, `test_exception_output.py` для UART-контракта, `test_fault_site.py` для связи symbol → исполняемые opcode-байты и `test_qemu_process.py` для жизненного цикла QEMU. В текущем `host-python` **39/39 unittest PASS**, включая восемь проверок остановки процесса; это отдельный счётчик от 23 этапов полного runner.

Исторический ARM ESR `0x96000061` host-декодер узнаёт как `alignment-fault` (Data Abort текущего EL). Это тест классификатора на сохранённом числе. Реальный Data Abort новым обработчиком в T-P1.2 не провоцировался и не объявляется проверенным.

### Сборка и форматы

```sh
python3 tools/build.py --arch x86_64 --output build
python3 tools/build.py --arch aarch64 --output build
```

x86 получается как EFI application PE32+ AMD64, без целевой libc, с entry UEFI. AArch64 получается как ELF для анализа и raw Image с ARM64 header для QEMU `virt`. Проверяются наличие/формат целевых артефактов и завершающие статусы инструментов.

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

`--test-exception` взаимоисключающий с `--invalid-boot-info` и `--invalid-memory-map`; обычный корневой `build` нельзя явно выбрать выходом exception-сборки. Trigger отсутствует в обычных образах. В тестовом образе собственные векторы/IDT уже установлены, handoff валиден и карта дошла до `MEMMAP PASS`; только затем исполняются `UD2` либо `BRK #0x120`.

Три реальные QEMU-проверки — `qemu-x86_64-exception`, `qemu-aarch64-exception`, `qemu-aarch64-exception-el2`. Каждая требует ровно одну полную строку `STYLEOS EXCEPTION arch=...`, затем отдельный `STYLEOS EMERGENCY HALT`, без `STYLEOS KERNEL HALT`. В строке ARM ожидаются EC `0x3c`, `reason=brk`, ESR/ELR/SPSR и `far=absent`; в строке x86 — vector 6, `reason=invalid-opcode`, `error=absent cr2=absent`. Служебный ноль нормализации кадра не считается аппаратным error code.

Сохранённый PC сверяется с `fault-site.json`, созданным из реального test executable и символа `styleos_test_fault`. ARM ELF и raw Image должны содержать opcode `002420d4` (`BRK #0x120`) на нужном месте. PE должен содержать `0f0b` (`UD2`) в исполняемой секции; runtime RIP равен **загруженному ImageBase + RVA**. Preferred ImageBase не подменяет фактический адрес загрузки.

В подтверждённом Linux-прогоне x86 saved RIP `0x602db42` совпал с runtime base `0x6029000` + RVA `0x4b42`; preferred base был `0x140000000`. ARM EL1 и EL2 → EL1 дали saved PC/ELR `0x40083908`, ESR `0xf2000120` и `far=absent`. Эти адреса относятся к конкретной сборке: после изменения кода runner вычисляет новый ожидаемый адрес из нового образа.

После полного терминального маркера harness ещё **200 мс** наблюдает процесс. Самостоятельный exit (включая код 0), reset, неполная строка, отсутствие маркеров или один timeout не проходят. Затем harness штатно останавливает живой QEMU; возврат 0 после запрошенного SIGTERM допустим. Состояние наблюдения и остановки записывается в журнал. Host-регрессии проверяют эти различия, чтобы успешная строка перед аварийным выходом не создавала ложный PASS.

Собственный обработчик работает только после своей точки установки; broken stack/UART, firmware GDT, неподдерживаемые асинхронные пути и поведение повторного сбоя имеют пределы [ADR-0005](adr/0005-early-exception-diagnostics.md). Результат BRK/UD2 не доказывает обработку всех EC/векторов или восстановление исполнения.

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

Полный Linux-прогон T-P1.2: **23/23 этапа PASS**, код завершения 0. Это пять host-наборов, три этапа воспроизводимости, четыре сборочных этапа QEMU и 11 сценариев QEMU. Воспроизводимость охватывает четыре обычных артефакта. Все прежние normal/negative memory/handoff сценарии, EL2 → EL1 и ARM64 128/256 МиБ сохранены. До изменений отдельно подтверждена исходная база T-P1.1: 18/18 этапов PASS.

Отдельный ARM64-only прогон T-P1.2 в **Linux**: **16/16 этапов PASS** (пять host-наборов, четыре сборки только ARM64 и семь ARM64-сценариев QEMU). Для проверки независимости OVMF CODE/VARS, `lld-link` и x86 QEMU были намеренно недоступны выбранному маршруту. Это проверка маршрутизации инструментария в Linux, а не результат телефона.

| Среда | Статус T-P1.2 | Граница результата |
|---|---|---|
| Linux, полный QEMU/OVMF x86-64 и AArch64 | PASS, 23/23 | Включает реальные UD2 и ARM BRK EL1/EL2, host Python 39/39 |
| Linux, ARM64-only без OVMF/x86 tools | PASS, 16/16 | Не доказывает работу текущего этапа в Android |
| Android/Termux владельца, QEMU 11.1.3 | PASS, 16/16 | [Отдельный отчёт и UART](test-results/t-p1-2/termux/README.md) |
| VirtualBox и физическое оборудование | SKIPPED | Отдельные реальные устройства/гипервизор в этом этапе не проверены |

Старые [T-P1.0](test-results/t-p1-0/README.md) и [T-P1.1](test-results/t-p1-1/README.md) сохраняются как история. На телефоне владельца T-P1.1 ранее дал [12 этапов PASS](test-results/t-p1-1/termux-arm64.md); телефонный коммит `1a22e961946c4b08e058d65d91955271c4a502c4` отличается от принятой документальной базы `e4823f2eaf1bfbdca6ea7a93768b76a3b289523b` только документацией. Эти факты не засчитывают новые exception-обработчики T-P1.2 как проверенные на Android.

Для нового Termux-прогона владельца из checkout `feature/styleos-t-p1-2`:

```sh
git rev-parse HEAD
qemu-system-aarch64 --version
CC=clang python tools/test.py --host --qemu --qemu-arch aarch64 --output build/termux
```

Runner сам завершает QEMU и не требует Ctrl+A. Ожидаются 16 этапов выбранного ARM64-маршрута; команда не запускает x86 и сравнение повторяемости. Сохраните полный новый `build/termux/test-results/results.json` и UART-журналы, включая две exception-проверки BRK с начальным EL1/EL2. Полученный от владельца results.json и оба UART-журнала опубликованы в [отчёте Termux](test-results/t-p1-2/termux/README.md).


## Риски и откат

Работа T-P1.2 остаётся в `feature/styleos-t-p1-2` от принятой базы `e4823f2`. Обновление `main`, merge, release и изменения Portable-os запрещены этой задачей.

Ошибки UEFI handoff, неправильный стек, неверные physical pointers и неподходящая firmware могут остановить boot до диагностики. Отсутствующий UART может скрыть результат, даже если CPU выполняется. Тесты должны фиксировать конфигурацию и отделять отсутствие вывода от ожидаемого halt.

Проверки работают с выходными образами и временными копиями firmware VARS. Личные диски/разделы компьютера не нужны. Для исследования старого состояния создаётся отдельная ветка/копия из согласованного коммита; откат опубликованного изменения выполняется новым `git revert` после решения владельца. История `main` не переписывается. Старый проект доступен по исходному SHA, архивной метке и в независимом Portable-os.
