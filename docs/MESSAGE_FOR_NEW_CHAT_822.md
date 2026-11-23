Продолжаем StyleOS, репозиторий LPYURA2/OS.

Принятая база S821: 472c9c0e622084ac3ad584f6256d1008c8df1597,
CI SUCCESS https://github.com/LPYURA2/OS/actions/runs/45259805320 .
Ветка 822: feature/styleos-acpica-namespace-init-822.

Прочитай AGENTS.md, docs/PROJECT.md, docs/NEXT_CHAT_822.md,
docs/architecture/ACPICA_SYNTHETIC_GUEST_AML_822.md.

S822 добавляет настоящий AcpiEvaluateObject официальной ACPICA
20260930 (лицензия BSD-3-Clause) только для синтетических
DSDT T822=0x822 и SSDT S822=0x2345 внутри EFI-гостя.
Никакие методы прошивки и аппаратные MMIO/PCI/DMA не разрешать.
Принятие только после точного CI SUCCESS с пятью реальными QEMU,
UART, ZIP, ISO и SHA.

Дальше реальные ACPI hostbridge _CRS/_SEG/_BBN, ECAM,
IOMMU ownership, реальная аппаратная DMA-изоляция, UEFI runtime,
production CR3 и совместный запуск ядра, Desktop, Processes,
.tea, StyleFS. Фундамент НЕ завершён.

После фундамента идём по согласованному продуктовому плану.
Не меняй main, старые ветки, Desktop, палитру и макеты.
