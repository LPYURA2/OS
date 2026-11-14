# StyleOS — технические проверки и отчёты

Здесь собраны ссылки на воспроизводимые команды, фактические результаты и границы проверок. Отчёты привязаны к проверенным исходникам. Тесты на хосте, загрузка в QEMU, QEMU в Android/Termux, VirtualBox и нативная загрузка — разные результаты.

## Сборка и запуск

- [Сборка, команды тестов и ручной запуск](https://github.com/LPYURA2/OS/blob/8bc7e78592c1aef78429042658f9ae02f71637c5/docs/TESTING.md) — технический протокол T‑P1.4. Это закреплённая версия протокола. Для более позднего результата Termux смотри дополнение ниже.
- [Ручной сценарий VirtualBox](https://github.com/LPYURA2/OS/blob/8bc7e78592c1aef78429042658f9ae02f71637c5/README.md#ручной-запуск-и-virtualbox) — EFI, виртуальный диск и журнал COM1. Сценарий описан, успешный прогон в VirtualBox не подтверждён.
- [Устройство и запуск T‑P1.0](PROTOTYPE_T-P1.0.md) — прежнее техническое описание из корневого README, сохранённое отдельно.
- [Архитектурные решения](https://github.com/LPYURA2/OS/tree/8bc7e78592c1aef78429042658f9ae02f71637c5/docs/adr) и [учебная карта кода](https://github.com/LPYURA2/OS/blob/8bc7e78592c1aef78429042658f9ae02f71637c5/docs/LEARNING_RU.md).

## Отчёты по этапам

| Этап | Что проверялось | Фактический отчёт |
|---|---|---|
| T‑P1.0 | Минимальная загрузка и передача управления общему ядру | [Отчёт](test-results/t-p1-0/README.md) |
| T‑P1.1 | Диагностическая карта памяти | [Отчёт](https://github.com/LPYURA2/OS/blob/8bc7e78592c1aef78429042658f9ae02f71637c5/docs/test-results/t-p1-1/README.md) |
| T‑P1.2 | Ранние исключения и контролируемая остановка | [Отчёт](https://github.com/LPYURA2/OS/blob/8bc7e78592c1aef78429042658f9ae02f71637c5/docs/test-results/t-p1-2/README.md) |
| T‑P1.3 | Пул страниц внутри образа ядра | [Отчёт](https://github.com/LPYURA2/OS/blob/8bc7e78592c1aef78429042658f9ae02f71637c5/docs/test-results/t-p1-3/README.md) · [Termux](https://github.com/LPYURA2/OS/blob/8bc7e78592c1aef78429042658f9ae02f71637c5/docs/test-results/t-p1-3/termux/README.md) |
| T‑P1.4 | Собственные таблицы страниц, алиас и аппаратный отказ трансляции | [Отчёт Linux/QEMU, UART и JSON](https://github.com/LPYURA2/OS/blob/8bc7e78592c1aef78429042658f9ae02f71637c5/docs/test-results/t-p1-4/README.md) · [Поздний прогон Termux](#androidtermux-t-p14) |

## Android/Termux T-P1.4

**27/27 PASS, код завершения 0.** Контрольный прогон владельца подтверждён двумя скриншотами консоли, на которых видны проверенный SHA, версия QEMU и все строки сводки.

| Параметр | Наблюдение |
|---|---|
| Исходники | [`68314ee6be30096e70267e6b003fd4cc771dba0e`](https://github.com/LPYURA2/OS/commit/68314ee6be30096e70267e6b003fd4cc771dba0e) |
| Среда | Android/Termux, QEMU AArch64 11.1.3 |
| Набор | 7 host-проверок, 7 сборок и 13 запусков QEMU |
| Новые сценарии | `qemu-aarch64-paging`, `qemu-aarch64-paging-fault`, `qemu-aarch64-paging-el2` и `qemu-aarch64-paging-fault-el2` — PASS |
| Границы доказательства | Исходные UART/JSON с телефона не приложены. Результат подтверждает ARM64 в эмуляторе. VirtualBox и нативное оборудование остаются SKIPPED. |

Запуск из checkout указанного коммита:

```sh
CC=clang python tools/test.py --host --qemu --qemu-arch aarch64 --output build/termux-tp14
```

Статус `SKIPPED` в закреплённом отчёте T‑P1.4 описывает состояние **до** этого контрольного прогона. Прежний отчёт оставлен без переписывания.
