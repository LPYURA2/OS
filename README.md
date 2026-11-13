# StyleOS — T-P1.3

**Начальный загрузочный прототип на реальных ISA x86-64 и AArch64 с общим минимальным C-ядром.**

StyleOS развивается в `LPYURA2/OS`. После handoff T-P1.0, диагностической карты памяти T-P1.1 и терминальной диагностики исключений T-P1.2 этап T-P1.3 добавляет внутренний пул из восьми страниц по 4096 байт. Все страницы уже находятся в загруженном образе ядра. Пул не получает память из `MEMMAP CANDIDATE` и не превращает всю RAM в собственность ядра.

Это ещё не пользовательская ОС: процессов, общей выдачи физических страниц, heap, файловой системы, графики и приложений здесь нет. Микроядерная, гибридная или монолитная модель не выбрана. Окончательные инженерные решения принимает владелец.

Историческая MATRYOSHKA v0.3.2, её Python VM, ISA M32/M8, ASM-ОС и `engine_lab` сохранены в [LPYURA2/Portable-os](https://github.com/LPYURA2/Portable-os). Они не являются ядром StyleOS. Portable-os этим этапом не меняется. Исходный коммит [`c5b62a4b4fc19d6db25d53f52eebcfba89dae708`](https://github.com/LPYURA2/OS/commit/c5b62a4b4fc19d6db25d53f52eebcfba89dae708) сохранён в Git-истории; подробности — [HISTORY.md](docs/HISTORY.md).

## Что выполняет ядро

| Общая часть | x86-64 | AArch64 |
|---|---|---|
| `kernel_main()` и `boot_info` v1, 96 байт | UEFI PE/COFF EFI application | raw ARM64 Image для QEMU `virt`, загрузка `0x40080000` |
| Проверка handoff и явный отказ | `GetMemoryMap` → `ExitBootServices`, собственный стек | DTB из `x0`, поддерживаемый EL1 или EL2 → EL1, собственный стек |
| Диагностическая карта RAM и резервов | Дескрипторы UEFI и реальный Loaded Image | Поддерживаемые memory/reservation данные DTB, linker bounds образа |
| Ранняя терминальная exception-диагностика | Собственная IDT; vector, saved RIP, error code при наличии | Собственные EL1 vectors; ESR/ELR/SPSR, достоверный FAR |
| Один фиксированный пул внутри образа | Полные границы PE/RVA и runtime Loaded Image | ELF/BSS/PT_LOAD и полный размер raw Image |
| Ранний UART и halt | COM1, port I/O после EBS | PL011 MMIO `0x09000000`, polling без IRQ |

Python собирает, упаковывает и проверяет образы. Функции ядра исполняются как скомпилированный машинный код целевой ISA; Python не заменяет ядро.

## Цепочки загрузки

```text
x86-64:
UEFI → GPT/FAT → EFI/BOOT/BOOTX64.EFI
 → efi_main → GetMemoryMap → ExitBootServices
 → собственный стек и IDT → kernel_main(boot_info)
 → проверка handoff → MEMMAP PASS → обычный KERNEL HALT

AArch64:
QEMU virt → Image 0x40080000, x0 = DTB
 → стартовый ASM → EL1, стек, очистка BSS, UART и VBAR_EL1
 → проверка DTB → kernel_main(boot_info)
 → проверка handoff → MEMMAP PASS → обычный KERNEL HALT
```

x86-образ включает и UEFI-адаптер, и общую C-часть. После `ExitBootServices` он не вызывает Boot Services и не использует UEFI console. Сохраняются унаследованные firmware mapping и GDT: собственные таблицы трансляции, GDT/TSS/IST и аварийный стек ещё не созданы. Это одна из причин не выдавать диагностические кандидаты RAM.

ARM использует соглашение ARM64 Image, а не Linux kernel code или сервисы Linux. Дополнительные CPU паркуются, IRQ маскированы. Поддержка QEMU `virt` с фиксированным PL011 не доказывает поддержку аппаратуры телефона или произвольной ARM-платы.

## Контракт и карта памяти

[boot_info.h](include/styleos/boot_info.h) задаёт неизменённый контракт v1: 96 байт, выравнивание 8, magic/version/size, платформа, архитектура и параметры исходных данных. Числовая проверка адреса не доказывает доступность произвольной памяти. Раскладка и границы — [ADR-0002](docs/adr/0002-boot-handoff-v1.md).

`kernel/boot_memory.c` нормализует полуинтервалы `[start, end)`, резервам отдаётся приоритет над RAM. `kernel/boot_memory_diagnostic.c` публикует карту только после полного успеха. `MEMMAP PASS` означает построение карты; `CANDIDATE` остаётся диагностическим результатом. Внутренний read-only снимок передаёт уже проверенные границы образа, стека, handoff, источника карты и рабочей памяти следующему этапу без изменения `boot_info` или правил нормализации. См. [ADR-0004](docs/adr/0004-early-physical-memory-proposal.md).

## Пул страниц T-P1.3

`kernel/boot_pool.c` содержит выровненный BSS-массив **8 × 4096 = 32768 байт**. Отдельный выровненный объект метаданных занимает 4096 байт и никогда не выдаётся как страница. Число 4096 относится к этому эксперименту, а не утверждает будущие гранулы MMU.

Допуск после валидных handoff и карты проверяет весь полуинтервал пула и метаданных: размер, переполнение, выравнивание, положение внутри живого зарезервированного образа и отсутствие перекрытий со стеком, handoff, исходной UEFI-картой или DTB, рабочими метаданными и друг с другом. При отказе пул не выдаёт страницы и не пишет в буфер.

Внутренний API рассчитан на один CPU, один пул и одну страницу за запрос. Он выдаёт **адрес внутри отображённого образа ядра**, обнуляет все 4096 байт при каждой выдаче и различает исчерпание, неверный адрес, невыровненный адрес и повторное освобождение. Отказ не меняет состояние выдачи. Это не общий PMM, не heap, не защита от use-after-free после повторной выдачи и не SMP-интерфейс.

Сопоставление адресов опирается на узкий платформенный контракт. ARM Image выполняется по фиксированному адресу с MMU off. На x86 в поддерживаемом UEFI/OVMF-профиле сохраняется плоское identity-отображение загрузки; runner сверяет runtime ImageBase с RVA символов из связанной сборки. Приведение произвольного C-указателя не объявляется переводом в физический адрес. Полные правила и бинарные доказательства — [ADR-0006](docs/adr/0006-image-owned-boot-pool.md).

Проверочная запись/чтение включаются только отдельным флагом:

```sh
python3 tools/build.py --arch all --test-bootpool
# По умолчанию отдельный каталог build/bootpool.
python3 tools/build.py --arch aarch64 --test-bootpool --output build/pool-arm64
```

После `MEMMAP PASS` тест выводит полные границы, выдаёт восемь разных страниц, проверяет обнуление, запись/чтение всех байтов, исчерпание и повторную выдачу возвращённой страницы с обнулением. Только затем следуют `STYLEOS BOOTPOOL PASS` и обычный `STYLEOS KERNEL HALT`. Ошибка даёт `STYLEOS BOOTPOOL ERROR` без PASS. Обычный образ не запускает самотест.

## Ранние исключения T-P1.2

Первая поддерживаемая синхронная ошибка после установки своих обработчиков печатает `STYLEOS EXCEPTION arch=...` и `STYLEOS EMERGENCY HALT`. Возврата к инструкции нет. Saved PC и адрес обращения печатаются без разыменования; недостоверные поля равны `absent`. Повторный вход сразу останавливается. Повреждённые стек/UART и сбои до установки обработчиков не покрыты обещанием надёжного вывода.

Отдельный `--test-exception` создаёт образы с реальными `UD2` и `BRK #0x120` после `MEMMAP PASS`; по умолчанию они находятся в `build/exception`. `fault-site.json` связывает saved PC с opcode в ELF/raw Image или с фактическим runtime PE ImageBase + RVA. Детали — [ADR-0005](docs/adr/0005-early-exception-diagnostics.md).

Флаги `--test-bootpool`, `--test-exception`, `--invalid-boot-info` и `--invalid-memory-map` взаимоисключающие. Тестовые режимы не смешиваются с обычным образом и друг с другом.

## Сборка и проверка в Linux

Нужны Python 3, Clang и LLVM LLD (`lld-link` для PE/COFF, `ld.lld` для ELF). Для полной QEMU-проверки нужны `qemu-system-x86_64`, `qemu-system-aarch64` и внешние совместимые OVMF CODE/VARS. Make необязателен. Целевая libc, root и сеть самой сборке не нужны.

Один из способов установки host-инструментов в Debian/Ubuntu:

```sh
sudo apt-get update
sudo apt-get install python3 clang lld make qemu-system-x86 qemu-system-arm ovmf
```

Допустим toolchain в пользовательском каталоге: пути задаются `CLANG`, `LLD_LINK`, `LD_LLD`, `CC`, `QEMU_X86`, `QEMU_AARCH64`, `OVMF_CODE`, `OVMF_VARS`. Firmware и toolchain в Git не хранятся.

```sh
python3 tools/build.py --arch all --output build
python3 tools/test.py --host
python3 tools/test.py --repro
python3 tools/test.py --qemu
python3 tools/test.py --all --output build/t-p1-3
```

| Артефакт обычной сборки | Назначение |
|---|---|
| `build/x86_64/BOOTX64.EFI` | UEFI PE/COFF-приложение x86-64 |
| `build/x86_64/styleos.img` | GPT/FAT16-диск с EFI removable-media path |
| `build/aarch64/kernel.elf` | ELF для анализа AArch64 |
| `build/aarch64/styleos.bin` | Raw Image для QEMU `-kernel` |
| `build/manifest.json` | SHA-256, размеры и параметры сборки |
| `build/<arch>/pool-site.json` | Полные границы объектов пула, доказанные по связанным артефактам |

Воспроизводимость сравнивает четыре обычных артефакта в двух независимых каталогах с одинаковым toolchain. Иные версии компилятора могут создавать иной код. Бинарники, объектные файлы и firmware не коммитятся.

Протоколы, точные команды, lifecycle QEMU и смысл статусов описаны в [TESTING.md](docs/TESTING.md). Фактические результаты нового этапа публикуются в [отчёте T-P1.3](docs/test-results/t-p1-3/README.md). Сохранённые [T-P1.2](docs/test-results/t-p1-2/README.md), [T-P1.1](docs/test-results/t-p1-1/README.md) и [T-P1.0](docs/test-results/t-p1-0/README.md) относятся к своим исходникам и не заменяют новый прогон.

Принятая база T-P1.2 — [`07fe425a0d4d5c0b9fbb65f8a37b30924523ec81`](https://github.com/LPYURA2/OS/commit/07fe425a0d4d5c0b9fbb65f8a37b30924523ec81). Она имеет подтверждённые 23/23 Linux-этапа и отдельные 16/16 Android/Termux; [телефонный отчёт](docs/test-results/t-p1-2/termux/README.md) относится к исходникам `ce24525169e1d47ea8dc2d2d1f63f767dbc54890`, после которых менялась только документация. **T-P1.3 на Android/Termux — SKIPPED**, пока владелец не выполнит новый прогон.

## Termux: новый каталог и закреплённый коммит

Termux — host для сборки и эмуляции. Это не установка StyleOS вместо Android и не испытание нативной загрузки телефона. Root и изменение разделов Android не нужны.

```sh
pkg install python clang lld git qemu-system-aarch64-headless
git clone --branch feature/styleos-t-p1-3 --single-branch https://github.com/LPYURA2/OS.git styleos-tp13
cd styleos-tp13
git checkout --detach f10ee3313b7d8d428a3207e5bcfff6b93842be09
git rev-parse HEAD
qemu-system-aarch64 --version
CC=clang python tools/test.py --host --qemu --qemu-arch aarch64 --output build/termux
```

Используйте новый каталог, оставив предыдущие результаты как историю. Этот маршрут не требует OVMF, `lld-link` и x86 QEMU. Runner сам завершает VM и сохраняет `build/termux/test-results/results.json` и журналы. Передайте владельцу полный набор новых журналов, особенно `qemu-aarch64-bootpool.log`, `qemu-aarch64-bootpool-el2.log` и две exception-проверки. Linux ARM64-only не засчитывается как результат Android.

## Ручной запуск и VirtualBox

Обычный ARM64-образ:

```sh
qemu-system-aarch64 -machine virt -cpu cortex-a57 -m 128M \
  -accel tcg -net none -nographic -kernel build/aarch64/styleos.bin
```

После handoff и карты ожидается `STYLEOS KERNEL HALT`; `Ctrl+A`, затем `X` завершает ручной QEMU. Самотест запускается тем же способом из отдельного `build/bootpool/aarch64/styleos.bin`. Один timeout без обязательных маркеров не является PASS.

x86 требует EFI и внешнюю OVMF, а не legacy BIOS или `qemu -kernel BOOTX64.EFI`. Ручная команда — в [TESTING](docs/TESTING.md). Для VirtualBox с подходящим x86-64 host можно преобразовать диск и выбрать EFI:

```sh
VBoxManage convertfromraw build/x86_64/styleos.img build/x86_64/styleos.vdi --format VDI
VBoxManage createvm --name StyleOS-TP13 --ostype Other_64 --register
VBoxManage modifyvm StyleOS-TP13 --firmware efi64 --memory 256 --cpus 1 \
  --uart1 0x3F8 4 --uartmode1 file /absolute/path/styleos-com1.log
VBoxManage storagectl StyleOS-TP13 --name SATA --add sata --controller IntelAhci
VBoxManage storageattach StyleOS-TP13 --storagectl SATA --port 0 --device 0 \
  --type hdd --medium /absolute/path/build/x86_64/styleos.vdi
VBoxManage startvm StyleOS-TP13
```

Замените пути и имя VM на свои; после halt выключите VM. Ранний вывод идёт в COM1, рабочего стола на VGA нет. VirtualBox и физические устройства для T-P1.3 — **SKIPPED** до отдельного реального испытания.

## Карта проекта

```text
OS/
├── AGENTS.md                 правила и границы этапов
├── Makefile                  оболочки сборки и тестов
├── README.md                 этот документ
├── include/styleos/          boot_info, HAL, memory evidence, pool, exceptions
├── kernel/                   общее ядро, карта, пул и отдельный самотест
├── boot/x86_64/              UEFI, GetMemoryMap/ExitBootServices
├── arch/x86_64/              стек, COM1, IDT/exception entry, halt
├── boot/aarch64/             Image/startup, linker, DTB/handoff
├── arch/aarch64/             PL011 и halt
├── tools/                    сборка, GPT/FAT, бинарные доказательства, runner
├── tests/                    host C/Python проверки
└── docs/
    ├── adr/                  решения T-P1.0–T-P1.3
    ├── HISTORY.md            архив MATRYOSHKA и Portable OS
    ├── LEARNING_RU.md        учебная карта кода
    ├── TESTING.md            воспроизводимый протокол
    └── test-results/         раздельные журналы каждого этапа
```

Назначение главных файлов и упражнения по настоящему коду — [LEARNING_RU.md](docs/LEARNING_RU.md).

## Ограничения и следующий шаг

Обычный образ не запускает тестовую выдачу; специальный образ проверяет только фиксированный пул внутри уже загруженного ядра. Общий PMM/heap, передача владения RAM, собственные таблицы трансляции, recovery исключений, IRQ, планировщик, процессы, IPC, драйверная модель, ФС, GUI, `.tea`, APK и EXE не реализованы. Пул не доказывает промышленную безопасность, изоляцию или загрузку на физическом оборудовании.

Сохранность данных, разграничение доступа, восстановление после сбоев, измеряемая производительность и явно ограниченные модели совместимости остаются целями StyleOS. Следующий этап требует решения владельца о владении общей памятью и сохранности переходных структур либо о переходе к собственным таблицам трансляции.

T-P1.3 публикуется только в `feature/styleos-t-p1-3`: **без обновления main, merge, force push, релиза или изменений Portable-os**. После отчёта работа останавливается для согласования дальнейшей архитектуры.
