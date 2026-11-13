# T-P1.0 — AArch64 в Android/Termux

Дата прогона: 13.11.2026. Исходники: `dfc5be9c2f699928a42903e70066f1bd8b20f65a` (`git rev-parse HEAD` на телефоне). Среда: телефон владельца, Android/Termux как хост, QEMU AArch64 `virt`, `cortex-a57`, 128 МиБ. Модель телефона и версии инструментов по предоставленным материалам не подтверждены.

Источник — скриншоты Termux со сборкой, запусками и выводом файлов журналов. Ниже приведены диагностические строки, прочитанные со скриншотов. Это не копии исходных файлов логов и не прогон `tools/test.py --all`.

| Сценарий | Итог | Наблюдение |
|---|---|---|
| Сборка AArch64 в Termux | PASS | `BUILD PASS: aarch64` для обычного и отрицательного образов. |
| Обычная загрузка | PASS | Старт в EL1, проверка DTB header и `boot_info v1`, handoff, halt. |
| Старт с EL2 | PASS | `initial EL2` → `current EL1`, handoff, halt. |
| Неверный magic | PASS | `BOOT_INFO ERROR: magic`, без `HANDOFF PASS`. |

## Обычная загрузка — termux-arm64.log

```text
STYLEOS AARCH64: initial EL1
STYLEOS AARCH64: current EL1
STYLEOS AARCH64: DTB header OK
STYLEOS kernel_main entered
STYLEOS boot_info validated v1
STYLEOS HANDOFF PASS aarch64
STYLEOS KERNEL HALT
```

После `KERNEL HALT` владелец завершил процесс QEMU через SIGTERM.

## Старт с EL2 — termux-arm64-el2.log

```text
STYLEOS AARCH64: initial EL2
STYLEOS AARCH64: current EL1
STYLEOS AARCH64: DTB header OK
STYLEOS kernel_main entered
STYLEOS boot_info validated v1
STYLEOS HANDOFF PASS aarch64
STYLEOS KERNEL HALT
```

Образ тот же, что в обычном запуске. Для QEMU задано `-machine virt,virtualization=on`.

## Неверный magic — termux-arm64-invalid.log

```text
STYLEOS AARCH64: initial EL1
STYLEOS AARCH64: current EL1
STYLEOS AARCH64: DTB header OK
STYLEOS kernel_main entered
STYLEOS BOOT_INFO ERROR: magic
```

Этот образ собран с `--invalid-boot-info` в отдельном `build/negative`. Оба дополнительных запуска ограничивались `timeout 15s`. Результат оценён по UART, а не по коду завершения команды с `tee`. Сообщение оболочки `Terminated` не является ошибкой ядра.

Не проверялись: host-тесты и воспроизводимость в Termux, x86-64 в Termux, VirtualBox и нативная загрузка физического оборудования. [Основной отчёт T-P1.0](README.md).
