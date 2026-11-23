# Передача нового чата — StyleOS после 816

Продолжаем разработку StyleOS, репозиторий `LPYURA2/OS`.
Последняя принятая до 816 база: этап 815,
`b6a84c48c58434dea15aac42f4fa68ed7017451d`.

Ветка 816: `feature/styleos-acpi-literal-namespace-816`.
Перед работой проверь точный финальный SHA и **полный SUCCESS CI** этой ветки,
сверь полный ZIP исходников, штатный ISO, подлинные UART/DSDT/SSDT и SHA.
Изучи AGENTS.md, docs/PROJECT.md, docs/NEXT_CHAT_816.md и
`docs/architecture/ACPI_LITERAL_NAMESPACE_816.md`, утверждённые макеты.

Этап 816 — ограниченный тестовый AML reader. Проверяет DSDT+SSDT,
Scope/Device/Name и только `Method(0){Return(literal)}` в синтетических
проверках, строго запрещает OperationRegion и неподдерживаемые выражения.
QEMU/OVMF проверяют реальные таблицы, не доказывают полноценное выполнение
ACPI _CRS/_SEG/_BBN или физическую принадлежность IOMMU.
DMA, UEFI runtime и production CR3 не включаются.

Следующий шаг — полный безопасный AML-движок с контролируемыми
OperationRegion/SSDT, настоящая проверка hostbridge _CRS/_SEG/_BBN и
резервирования ECAM, IOMMU requesters/aliases/exceptions, потом реальные
DMA deny/allow с fault/invalidation. Далее UEFI runtime, собственный CR3
и интеграционная аппаратная приёмка ядра вместе с Desktop, процессами,
`.tea` и StyleFS. **Фундамент StyleOS НЕ ЗАВЕРШЁН.**

После полного фундамента — «Процессы» → независимые `.tea` → пользователи и
Welcome/Lock → StyleFS/Проводник с «Быстрым доступом» → установщик `.tea`,
App World, интернет, браузер, звук, VPN.
Ничего не менять в main, принятых ветках, Desktop, палитре и макетах.
Каждый этап — отдельная ветка с полным CI архивом, SHA, ISO, доказательствами,
техническим отчётом, точкой продолжения и сообщением следующему чату.
