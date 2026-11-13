# StyleOS — T-P1.0

**Начальный загрузочный прототип на реальных ISA x86-64 и AArch64 с общим минимальным C-ядром.**

StyleOS — отдельный проект в `LPYURA2/OS`. В T-P1.0 проверяется цепочка от аппаратного entry до `kernel_main(const struct boot_info *)`, ранняя serial-диагностика и контролируемая остановка. Это ещё не пользовательская ОС: процессов, аллокатора, файловой системы, графики и приложений здесь нет.

Историческая MATRYOSHKA v0.3.2, её Python VM, ISA M32/M8, ASM-ОС и `engine_lab` остаются в [LPYURA2/Portable-os](https://github.com/LPYURA2/Portable-os). Код Portable OS не является ядром StyleOS и при T-P1.0 не меняется. Исходная точка прежнего проекта — [`c5b62a4b4fc19d6db25d53f52eebcfba89dae708`](https://github.com/LPYURA2/OS/commit/c5b62a4b4fc19d6db25d53f52eebcfba89dae708), сохранённая в Git-истории. Подробности — [HISTORY.md](docs/HISTORY.md).

## Что реализует прототип

| Общая часть | x86-64 | AArch64 |
|---|---|---|
| `kernel/main.c`, одна функция `kernel_main()` | UEFI PE/COFF EFI application | raw Image для QEMU `virt`, загрузка `0x40080000` |
| `boot_info` v1, 96 байт | `GetMemoryMap` → `ExitBootServices`, ограниченные повторы | DTB из `x0`, проверка заголовка, поддерживаемый EL2/EL1 → EL1 |
| Проверка handoff и явный отказ | Собственный 64-КиБ стек и Microsoft x64 ABI | Собственный 64-КиБ стек, очистка BSS, ранние exception vectors |
| Узкий HAL для serial и halt | COM1, порт `0x3F8`, после EBS без вызовов firmware | PL011 MMIO `0x09000000`, polling без IRQ-драйвера |
| Общие C-исходники и тестовая граница | Машинный код x86-64 | Машинный код AArch64 |

Python используется инструментами сборки, создания GPT/FAT-образа и запуска тестов. Процессор и функции ядра StyleOS выполняются как скомпилированный машинный код целевой ISA.

Окончательная модель ядра — микроядерная, гибридная или монолитная — **не выбрана этим этапом**. Системные службы, production ABI, VM/MMU, аллокатор, планировщик, IPC, файловая система, драйверная модель и API приложений требуют отдельных решений владельца. См. [ADR-0001](docs/adr/0001-prototype-scope.md).

## Цепочки загрузки

### x86-64: UEFI → общее C-ядро

```text
UEFI firmware
  → GPT-диск / FAT-раздел / EFI/BOOT/BOOTX64.EFI
  → efi_main(): подготовка и проверка firmware boundary
  → GetMemoryMap() → ExitBootServices() с актуальным MapKey
  → собственный стек, ABI Microsoft x64
  → kernel_main(boot_info)
  → проверка handoff, COM1-диагностика, halt
```

`BOOTX64.EFI` включает и UEFI-адаптер, и общую C-часть. В T-P1.0 нет отдельного универсального ELF-загрузчика. Перед выходом сохраняются сырые дескрипторы карты памяти, их размер, шаг и версия. При изменившемся ключе карты выполняется ограниченный retry; остальные ошибки не маскируются под успешный вход ядра.

После успешного `ExitBootServices` нельзя пользоваться UEFI console, выделением памяти или другими Boot Services. Serial использует прямой port I/O, а собственный стек резервируется в образе. Карта UEFI передаётся как данные; готового аллокатора на её основе пока нет.

### AArch64: QEMU virt → EL1 → общее C-ядро

```text
QEMU virt, AArch64 Image boot convention
  → Image по 0x40080000, x0 = физический адрес DTB
  → стартовый ASM: сохранить x0, подготовить EL1 и стек
  → проверить заголовок DTB, сформировать boot_info
  → kernel_main(boot_info)
  → проверка handoff, PL011-диагностика, halt
```

Image использует загрузочное соглашение ARM64 Image, а не код или сервисы ядра Linux. При входе EL2 выполняется переход в EL1; EL1 entry также поддерживается. Дополнительные CPU паркуются, IRQ остаются маскированными. Ранний вектор ошибок даёт диагностику, но не является системой восстановления исключений.

DTB проверяется на уровне фиксированного заголовка. Полный разбор дерева, обнаружение устройств и построение карты памяти не реализованы. PL011 пока является фиксированным свойством адаптера QEMU `virt`, а не поддержкой произвольного телефона или платы ARM.

## Контракт `boot_info` v1 и минимальный HAL

[include/styleos/boot_info.h](include/styleos/boot_info.h) задаёт структуру **96 байт**, выровненную по 8, с `magic`, `version`, `size`, архитектурой, платформой и флагами. x86 передаёт физический адрес сырой карты UEFI и параметры дескрипторов; ARM — физический адрес DTB и EL ядра. Также передаётся положение раннего UART. Резервные поля равны нулю.

В структуре нет firmware callback, `MapKey` или C-указателей. Числовые проверки структуры не доказывают доступность любого физического адреса. Образ, стек и буферы handoff должны оставаться зарезервированными до появления менеджера памяти, который явно примет их. Полная раскладка и правила — [ADR-0002](docs/adr/0002-boot-handoff-v1.md).

[include/styleos/arch.h](include/styleos/arch.h) содержит только `arch_console_init()`, `arch_putc()`, `arch_puts()` и `arch_halt()`. Общему C-коду не нужны COM1-регистры, PL011-регистры или UEFI API. Это минимальная граница ранней диагностики, не завершённая универсальная HAL: [ADR-0003](docs/adr/0003-early-console-hal.md).

## Сборка в Linux

Нужны Python 3, Clang и LLVM LLD с `lld-link` для PE/COFF и `ld.lld` для ELF. Make — необязательная оболочка. Входной код freestanding; целевая libc и кросс-сборка Linux не нужны.

Пример подготовки Debian/Ubuntu, выполняемый владельцем в подходящем окружении:

```sh
sudo apt-get update
sudo apt-get install python3 clang lld make qemu-system-x86 qemu-system-arm ovmf
```

Команды с `sudo` не требуются самой StyleOS: это один из способов установить host-инструменты. В управляемой среде допустимо использовать официальные пакеты, распакованные в отдельный пользовательский каталог; пути задаются переменными `CLANG`, `LLD_LINK` и `LD_LLD`. Бинарники firmware и toolchain в Git не включаются.

Из корня репозитория:

```sh
python3 tools/build.py --arch all --output build
# Или только одна архитектура:
python3 tools/build.py --arch x86_64 --output build
python3 tools/build.py --arch aarch64 --output build
```

Результаты:

| Файл | Назначение |
|---|---|
| `build/x86_64/BOOTX64.EFI` | UEFI PE/COFF-приложение x86-64 |
| `build/x86_64/styleos.img` | Raw GPT/FAT16-диск с EFI removable-media boot path |
| `build/aarch64/kernel.elf` | ELF для анализа/отладки, не x86-программа |
| `build/aarch64/styleos.bin` | Плоский AArch64 Image для QEMU `-kernel` |
| `build/manifest.json` | Размеры и SHA-256 целевых артефактов, сведения о компиляторе |

Сборщик создаёт только выходной каталог и не требует сети. Повторяемость сравнивается для одинакового набора исходников и toolchain; другой компилятор может дать другой машинный код. Образы и объектные файлы не хранятся в исходном Git-дереве.

## Проверки и запуск QEMU

```sh
python3 tools/test.py --host
python3 tools/test.py --repro
python3 tools/test.py --qemu
# Полный набор:
python3 tools/test.py --all
```

Оболочки Makefile: `make`, `make host-test`, `make repro`, `make smoke`, `make test`. Подробности — [TESTING.md](docs/TESTING.md), фактические журналы — [T-P1.0 results](docs/test-results/t-p1-0/README.md). Runner возвращает 0 при PASS, 1 при FAIL и 2 при пропущенных обязательных средствах без FAIL.

Финальный прогон 13.11.2026 в Linux: **PASS** — host-проверки `boot_info`, 14 сценариев UEFI, 6 тестов GPT/FAT и 4 проверки классификации serial-логов; сборки обеих ISA; побайтная повторяемость четырёх артефактов; пять QEMU-сценариев (x86 обычный/ошибочный handoff, ARM EL1/EL2 и ошибочный handoff). **SKIPPED в Linux-прогоне** — VirtualBox, Android/Termux и физическое оборудование. Отдельная проверка ARM64 в Termux описана ниже. Это подтверждение начальной загрузки, а не готовой ОС.

Для ручного AArch64 boot с обычным QEMU:

```sh
qemu-system-aarch64 -machine virt -cpu cortex-a57 -m 128M \
  -nographic \
  -kernel build/aarch64/styleos.bin
```

Ожидаемая последовательность после успешного handoff включает:

```text
STYLEOS kernel_main entered
STYLEOS boot_info validated v1
STYLEOS HANDOFF PASS aarch64
STYLEOS KERNEL HALT
```

Затем CPU ожидает в halt-цикле. Завершите ручной QEMU `Ctrl+A`, затем `X`. Автоматический тест останавливает VM по своему ограничению времени и проверяет serial-журнал; штатная остановка прототипа не является зависанием полноценной ОС.

x86 требует внешнюю UEFI firmware OVMF. Конкретные CODE/VARS файлы и параметры автоматического smoke-теста перечислены в TESTING. Старты через legacy BIOS и `qemu -kernel` для `BOOTX64.EFI` не являются этим загрузочным путём.

## Termux как host для разработки

Речь идёт о сборке и, при наличии совместимого QEMU, эмуляции из Termux. Это не установка StyleOS вместо Android и не поддержка аппаратуры телефона. Root, прошивка телефона и изменение Android-разделов не нужны.

```sh
pkg install python clang lld make git -y
git clone https://github.com/LPYURA2/OS.git styleos-tp10
cd styleos-tp10
python tools/build.py --arch all --output build
```

Clang/LLD должны содержать оба нужных target backend и `lld-link`; проверьте наличие команд через `command -v clang lld-link ld.lld`. Доступность QEMU и OVMF зависит от текущих пакетов Termux: сначала используйте `pkg search qemu`, затем установите подходящий пакет отдельно. Названия Linux apt-пакетов нельзя автоматически переносить в Termux.

Отдельный прогон на телефоне владельца 13.11.2026: **PASS** для сборки AArch64 в Termux и трёх загрузочных сценариев в QEMU `virt` (EL1, EL2 → EL1, отказ при неверном `boot_info`). Host-тесты, воспроизводимость и x86-64 в Termux не проверялись. Нативная загрузка телефона также не проверялась. [Результаты Termux](docs/test-results/t-p1-0/termux-arm64.md).

## VirtualBox: воспроизводимый ручной сценарий x86-64

Прототип рассчитан на EFI-загрузку x86-64; VirtualBox с подходящим x86-64 host и EFI можно проверить отдельно. Сам этот сценарий — инструкция, а не результат испытания VirtualBox.

В каталоге с собранным `build/x86_64/styleos.img`:

```sh
VBoxManage convertfromraw build/x86_64/styleos.img build/x86_64/styleos.vdi --format VDI
VBoxManage createvm --name StyleOS-TP10 --ostype Other_64 --register
VBoxManage modifyvm StyleOS-TP10 --firmware efi64 --memory 256 --cpus 1 \
  --uart1 0x3F8 4 --uartmode1 file /absolute/path/styleos-com1.log
VBoxManage storagectl StyleOS-TP10 --name SATA --add sata --controller IntelAhci
VBoxManage storageattach StyleOS-TP10 --storagectl SATA --port 0 --device 0 \
  --type hdd --medium /absolute/path/build/x86_64/styleos.vdi
VBoxManage startvm StyleOS-TP10
```

Замените `/absolute/path/` на действительные пути и используйте новое имя VM, если такое уже есть. Диагностика идёт в COM1-файл; рабочего стола или текстовой shell на VGA не ожидается. После halt VM нужно выключить вручную средствами VirtualBox. AArch64 `virt`-образ не является диском для этой x86 VM.

Результат VirtualBox в T-P1.0 — **SKIPPED**, если не приложен журнал реального запуска. Физический PC, ARM-плата, смартфон и планшет также не подтверждены.

## Карта проекта

```text
OS/
├── AGENTS.md                 правила StyleOS и пределы задачи
├── Makefile                  оболочки сборки и тестов
├── README.md                 этот документ
├── include/styleos/          boot_info v1, интерфейс раннего HAL
├── kernel/                   общее C-ядро, валидация handoff, runtime минимум
├── boot/x86_64/              UEFI entry, типы EFI, GetMemoryMap/ExitBootServices
├── arch/x86_64/              собственный стек, COM1, halt
├── boot/aarch64/             Image entry, linker script, проверка DTB/handoff
├── arch/aarch64/             PL011 и halt
├── tools/                    build, GPT/FAT packaging, test orchestration
├── tests/                    host-тесты контрактов и образа
└── docs/
    ├── adr/                  причины решений T-P1.0
    ├── HISTORY.md            связь с архивной MATRYOSHKA и Portable OS
    ├── LEARNING_RU.md        разбор загрузки и учебные упражнения
    ├── TESTING.md            команды, границы доказательств и результаты
    └── test-results/t-p1-0/  фактические текстовые журналы и SHA-256 артефактов
```

Выходной `build/` и пользовательская firmware/toolchain находятся вне отслеживаемых исходников. Состав фактических файлов уточняется непосредственно деревом Git.

## Пределы и следующий шаг

Текущий kernel выводит результат проверки handoff и останавливается. Нет установки, загрузки пользовательских программ, процессов, аллокатора, VM/MMU-подсистемы, IRQ/GIC-драйвера, ФС, GUI, сетевого стека, `.tea`, APK или EXE. Включённый firmware adapter и ранняя exception-диагностика не обеспечивают secure boot, восстановление, изоляцию, обновление или промышленную безопасность.

Сохранность данных, разграничение доступа, восстановление после сбоев, измеряемая производительность и отдельные модели совместимости остаются обязательными целями будущей StyleOS. Они вводятся проверяемыми этапами. Окончательные архитектурные решения принимает владелец.

После T-P1.0 следующий шаг — рассмотрение фактических журналов обеих загрузок и выбор отдельной узкой задачи. Возможные темы для обсуждения: спецификация владения памятью и handoff, модель исключений, дальнейшая модель ядра. Автоматическая реализация этих тем не входит в T-P1.0.

Учебный разбор файлов, ISA/ABI, стека, DTB и различий между host-test и boot-smoke — [LEARNING_RU.md](docs/LEARNING_RU.md).
