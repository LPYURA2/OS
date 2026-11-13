# T-P1.1 — контрольный прогон Android/Termux

13.11.2026 на телефоне владельца. Проверяемый коммит — [`1a22e961946c4b08e058d65d91955271c4a502c4`](https://github.com/LPYURA2/OS/commit/1a22e961946c4b08e058d65d91955271c4a502c4). На скриншоте видно переключение HEAD на `1a22e96`, затем все 12 строк PASS и возврат в оболочку Termux.

Команда прогона:

```sh
CC=clang python tools/test.py --host --qemu --qemu-arch aarch64 --output build/termux
```

| Этап | Итог | Что проверяет |
|---|---|---|
| `host-boot-info` | PASS | Контракт handoff |
| `host-uefi` | PASS | Host-проверка логики UEFI, не загрузка x86-64 |
| `host-memory` | PASS | Парсеры и нормализация карты |
| `host-python` | PASS | Образ диска и классификация UART |
| `build-for-qemu` | PASS | Обычный AArch64 Image |
| `build-negative` | PASS | AArch64 Image с неверным magic |
| `build-negative-memory` | PASS | AArch64 Image с усечённым входом карты |
| `qemu-aarch64-normal` | PASS | EL1, карта для 128 МиБ |
| `qemu-aarch64-invalid` | PASS | Ожидаемый отказ handoff |
| `qemu-aarch64-invalid-map` | PASS | Ожидаемый отказ карты |
| `qemu-aarch64-el2` | PASS | Переход EL2 → EL1 |
| `qemu-aarch64-256m` | PASS | EL1, карта для 256 МиБ |

Источник — скриншот итогового вывода test runner. Исходные файлы UART, отдельный код завершения всей команды, версии Android/QEMU/компилятора и модель телефона со скриншота не подтверждены. Журналы создаются на устройстве в `build/termux/test-results/`.

Этот результат не подтверждает x86-64 QEMU в Termux, воспроизводимость в Termux, VirtualBox или нативную загрузку телефона. [Основной отчёт T-P1.1](README.md).
