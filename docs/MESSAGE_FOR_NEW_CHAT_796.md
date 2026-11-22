Продолжаем разработку StyleOS в репозитории `LPYURA2/OS`.

Последняя принятая база — этап 795, коммит
`2b5581f4726f47090bebe893a163f077416f9276`,
CI https://github.com/LPYURA2/OS/actions/runs/45046497532 — SUCCESS.

Этап 796: отдельная ветка `feature/styleos-pci-topology-inventory-796`.
Проверить финальный SHA и успешный GitHub Actions и оригинальные UART-журналы
перед признанием 796 принятым. Прочитать `AGENTS.md`, `docs/PROJECT.md`,
`docs/NEXT_CHAT_796.md`, `docs/architecture/PCI_TOPOLOGY_INVENTORY_796.md`,
`docs/design/APPROVED_UI_REFERENCES.md`.

796 выполняет только read-only-инвентаризацию PCI в отдельном EFI:
обход мостов/шин, обнаружение BAR-баз и окон мостов; два QEMU-режима
(обычный и с реальным PCIe root-port + downstream endpoint), строгие UART,
ASAN/UBSAN и COFF. Никаких операций PCI data write, выделения MMIO,
производственного CR3 или подключения драйверов. Размеры остальных BAR
неизвестны, даже при прохождении CI. Продолжить закрытие PAT/MTRR,
IRQ/APIC, EFI runtime и доказать рабочую полную карту памяти обычного Desktop.

Фундамент НЕ объявлять завершённым до интеграции и аппаратной проверки
вместе с обычным Desktop. Затем по утверждённому плану: полноценные
«Процессы» с реальными PID/CPU/RAM и вкладками; независимые `.tea`;
пользователи и вход по `StyleOS-Welcome-and-Lock-Screens.pdf`; StyleFS
и Проводник по `StyleOS-Explorer-Mockups.pdf` с разделом «Быстрый доступ»;
далее установщик, App World, сеть, браузер, звук и VPN.

Не изменять main и принятые ветки. Сохранить существующий Desktop,
палитру и утверждённые макеты. По каждому этапу выдавать полные исходники
из успешного CI, ISO, архив доказательств, точный SHA, ссылку Actions,
отчёт, точку продолжения и отдельное готовое сообщение для нового чата.
