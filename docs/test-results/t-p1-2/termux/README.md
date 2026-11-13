# T-P1.2 — контрольный прогон Android/Termux

13.11.2026. Проверенный HEAD — [`ce24525169e1d47ea8dc2d2d1f63f767dbc54890`](https://github.com/LPYURA2/OS/commit/ce24525169e1d47ea8dc2d2d1f63f767dbc54890). Хост — устройство владельца с Android/Termux. Эмулятор — QEMU **11.1.3**, AArch64 `virt` через TCG. Точные версии Android, Python и Clang владелец не передавал.

```sh
CC=clang python tools/test.py --host --qemu --qemu-arch aarch64 --output build/termux
```

**16/16 этапов PASS** по переданному [results.json](results.json): пять host-наборов, четыре сборки ARM64 и семь ARM64-сценариев QEMU. Это отдельный прогон, не часть 23 этапов Linux. Ниже приложены два UART-журнала, полученные текстом от владельца, с окончаниями строк в формате чата. Остальные журналы перечислены в JSON, но не приложены.

| Сценарий | Итог | Наблюдение |
|---|---|---|
| [BRK с начальным EL1](qemu-aarch64-exception.log) | PASS | `initial EL1`, `current EL1`, одна строка `EXCEPTION`, отдельный `EMERGENCY HALT`. |
| [BRK с начальным EL2](qemu-aarch64-exception-el2.log) | PASS | `initial EL2`, `current EL1`, те же диагностические поля и аварийная остановка. |

В обоих журналах `class=0x3c`, `reason=brk`, `pc=elr=0x400838f8`, `esr=0xf2000120`, `far=absent`, без обычного `KERNEL HALT`. PC здесь относится к сборке в Termux и отличается от адреса в Linux-сборке. Runner отмечает PASS для контракта исключений. SIGTERM отправлен runner после аварийного маркера, а не получен в результате сбоя QEMU.

Этот прогон не проверял новый Data Abort, x86-64 QEMU или воспроизводимость в Termux, VirtualBox и нативную загрузку устройства. [Основной отчёт T-P1.2](../README.md).
