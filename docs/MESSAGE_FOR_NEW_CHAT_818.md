# Сообщение для нового чата — StyleOS после 818

Продолжаем разработку StyleOS, LPYURA2/OS.

Принятая база: 817, коммит
ae33a3cceda1a70cb8df76cf62217eac3e655e58,
CI https://github.com/LPYURA2/OS/actions/runs/45226061994 SUCCESS.

Ветка 818: feature/styleos-acpica-osl-guard-818.
Перед продолжением проверь точный финальный SHA и полный CI в этой
ветке, ZIP исходников, ISO, оригинальные UART/DSDT/QEMU, SHA и
унаследованные тесты. Прочитай AGENTS.md, docs/PROJECT.md,
docs/NEXT_CHAT_818.md и docs/architecture/ACPICA_OSL_DENY_GATE_818.md.

818 — только freestanding модель безопасного доступа к проверенной
копии ACPI и безусловного отказа аппаратных AML OperationRegion:
MMIO, port IO, PCI_Config, vendor regions. Есть проверка эпохи,
границ, бюджета и отсутствие DMA grant. Это НЕ настоящее ACPICA,
встроенное в ядро, НЕ выполненный ACPI _CRS/_SEG/_BBN и НЕ DMA-изоляция.

Дальше: полноценная, проверенная интеграция ACPICA с безопасной OSL,
реальная ACPI namespace/SSDT/ECAM политика, IOMMU ownership и аппаратный
DMA deny/allow/fault/invalidation, UEFI runtime, собственный CR3.
Затем обязательная аппаратная проверка штатного ядра вместе с
Desktop, процессами, независимыми .tea, StyleFS и сохранением файлов.

Только после этой полной проверки фундамент считается завершённым.
Затем: Процессы → .tea → пользователи/Welcome/Lock → Проводник с
«Быстрым доступом» → установщик, App World, Интернет, браузер,
звук и VPN.

main, принятые ветки, Desktop, палитру и утверждённые макеты НЕ менять.
Отдельная ветка на этап, полный CI ZIP, ISO, оригинальные аппаратные
доказательства, SHA, отчёт и сообщение для нового чата.

**ФУНДАМЕНТ STYLEOS НЕ ЗАВЕРШЁН.**
