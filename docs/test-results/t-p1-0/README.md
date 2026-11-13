# T-P1.0 — фактические результаты

Финальный прогон выполнен **13.11.2026 UTC** в Linux x86-64:

```sh
python3 tools/test.py --all
```

Код завершения **0**. Все **13 проверочных этапов PASS**; это не число отдельных unit-тестов. Данные среды — [environment.json](environment.json), версии — [toolchain-versions.log](toolchain-versions.log), машиночитаемые результаты — [results.json](results.json).

Clang/LLD 19.1.7, GCC 14.2.0 для host-тестов, Python 3.12.14, QEMU 10.0.13, OVMF 2025.02. Компилятор, QEMU и OVMF взяты из официальных Debian-пакетов, распакованных в пользовательский каталог без root и изменения системной конфигурации. Абсолютные пути в журналах относятся к этой среде; переменные выбора инструментов описаны в [TESTING](../../TESTING.md).

| Этап | Статус | Доказательство |
|---|---|---|
| Host: структура/отказы boot_info | PASS | [C-тест](host-boot-info.log) |
| Host: UEFI map/EBS, 14 сценариев | PASS | [C-тест](host-uefi.log) |
| Host: GPT/FAT 6 + serial evidence 4 | PASS, 10/10 | [Python unittest](host-python.log) |
| Повторная сборка 1 | PASS | [Команды компиляции/линковки](repro-build-one.log) |
| Повторная сборка 2 | PASS | [Команды компиляции/линковки](repro-build-two.log) |
| Повторяемость четырёх артефактов | PASS | [SHA-256](reproducibility.log) |
| Обычная сборка двух ISA | PASS | [Сборка](build-for-qemu.log) |
| Negative-сборка двух ISA | PASS | [Сборка с испорченным magic](build-negative.log) |
| x86-64 UEFI → EBS → kernel_main | PASS | [Реальный QEMU/OVMF serial](qemu-x86_64-normal.log) |
| x86-64: отказ на неверном boot_info | PASS | [Реальный QEMU/OVMF serial](qemu-x86_64-invalid.log) |
| AArch64: начальный EL1 | PASS | [Реальный QEMU serial](qemu-aarch64-normal.log) |
| AArch64: отказ на неверном boot_info | PASS | [Реальный QEMU serial](qemu-aarch64-invalid.log) |
| AArch64: начальный EL2 → EL1 | PASS | [Реальный QEMU serial](qemu-aarch64-el2.log) |

VM запускаются через TCG, с 128 МиБ RAM и одним vCPU. Нормальный kernel печатает архитектурный PASS и `STYLEOS KERNEL HALT`; дальше он остаётся в архитектурном halt-цикле. Harness завершает QEMU сигналом после проверки serial-маркеров, поэтому сообщение QEMU `terminating on signal 15` в конце журнала ожидаемо. Программа гостя не выдаёт этот сигнал и не вызывает QEMU-specific exit.

Serial-журналы сохраняют исходные UART-окончания строк CRLF и управляющие последовательности firmware; они не отредактированы для удаления диагностических сообщений. Правило `.gitattributes` сохраняет этот формат только для текстовых журналов.

Negative-варианты намеренно повреждают magic до входа общего ядра. PASS теста означает получение `STYLEOS BOOT_INFO ERROR: magic` и отсутствие положительного handoff-маркера. Эти сборки не являются обычным образом StyleOS.

## Непроверенные среды

| Среда | Статус | Причина |
|---|---|---|
| VirtualBox EFI x86-64 | SKIPPED | `VBoxManage`/VirtualBox отсутствует; предоставлен ручной сценарий |
| Android/Termux | SKIPPED | Нет фактического запуска нового StyleOS toolchain/QEMU на Android |
| Физический PC UEFI | SKIPPED | Испытание на физическом компьютере не выполнялось |
| Физическое ARM64-оборудование | SKIPPED | Не предоставлены board/firmware и журнал запуска; `virt` не подтверждает телефон/плату |

## Исправленные проблемы проверки

Первоначальный запуск x86 завершился **FAIL до исполнения guest**: локальному QEMU не хватало `vgabios-stdvga.bin`. [Исходный журнал](initial-x86-vga-missing.log) сохранён. В финальном headless-сценарии явно задано `-vga none`; реальный последующий boot подтверждён журналами выше. Видеодрайвер/GUI этим не реализованы.

Независимое [review](review.md) обнаружило P2 в строгости runner: префикс `BOOT_ERROR` с подчёркиванием мог игнорироваться при последующих PASS-маркерах. Ошибка исправлена, добавлены четыре регрессии; исходный ошибочный поток теперь отвергается. Полный прогон выше выполнен после исправления. Review не выявило открытых блокеров в объёме T-P1.0.

## Что именно проверено

[tested-sources.json](tested-sources.json) фиксирует SHA-256 исходников, тестов и сборочных инструментов, использованных в финальном прогоне. [artifacts.json](artifacts.json) фиксирует размеры/хеши EFI, GPT-диска, ARM ELF и ARM Image. Сами бинарные образы и сторонняя firmware в Git не включены; они пересоздаются сборкой.

Повторяемость относится к одинаковому toolchain и исходникам. Проверка handoff не доказывает готовность менеджера памяти, процессов, файловой системы, GUI, безопасности, восстановления, `.tea`, APK/EXE или окончательного ABI/модели ядра. T-P1.0 завершается на диагностике и остановке CPU.
