# Сообщение для нового чата — StyleOS 814

Продолжаем разработку StyleOS, LPYURA2/OS.
Последняя принятая база перед 814 — 813:
`9e93e47a811cf7051279fe08e7670cba39bd32a2`.

Ветка этапа 814:
`feature/styleos-uefi-rootbridge-audit-814`.

Обязательно проверь текущий точный SHA, финальный SUCCESS GitHub
Actions и полные артефакты этого же SHA. Прочитай AGENTS.md,
docs/PROJECT.md, docs/NEXT_CHAT_814.md,
docs/architecture/UEFI_ROOTBRIDGE_RESOURCES_814.md и макеты StyleOS.

В 814 ограниченно сверяются UEFI PCI Root Bridge I/O resource
Configuration/SegmentNumber с ACPI MCFG 813 и настоящими сегмент-0
PCI шинами, обнаруженными через CF8/CFC. Профиль изолирован от
обычного Desktop. Важно: ЭТО НЕ АСPI _CRS/_SEG/_BBN AML evaluation,
не ECAM доступ и не доказательство полной IOMMU принадлежности.
DMA остаётся запрещённым.

Следующий этап должен проверить реальные ACPI host bridge _CRS/_SEG
и резервирование ECAM, затем IOMMU ownership / aliases /
exceptions, настоящие DMA deny/allow с faults/invalidation,
UEFI runtime и собственный CR3. Затем — полная интеграционная
аппаратная приёмка ядра, Desktop, процессов, .tea и StyleFS.

Порядок: фундамент → Процессы → независимые .tea → пользователи и
Welcome/Lock → StyleFS/Проводник с Быстрым доступом → установщик,
App World, интернет, браузер, звук, VPN.
ФУНДАМЕНТ STYLEOS НЕ ЗАВЕРШЁН.

Не изменять main, принятые ветки, Desktop, палитру, макеты.
Каждый новый этап — отдельная ветка, полный ZIP успешного CI,
ISO, аппаратные UART/JSON, SHA, GitHub Actions, технический отчёт,
точка продолжения и отдельное сообщение для следующего чата.
