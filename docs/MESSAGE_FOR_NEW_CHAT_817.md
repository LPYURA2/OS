# Сообщение для нового чата — StyleOS после 817

Продолжаем работу в LPYURA2/OS. Принятая база до 817 —
коммит 816: 99e8d873aba5725e77760d0360280763c0d14fb9,
CI https://github.com/LPYURA2/OS/actions/runs/45219189553 (SUCCESS).

Ветка 817: feature/styleos-acpica-replay-817.
Уточни финальный коммит и проверь GitHub Actions SUCCESS по этому SHA,
оригинальные доказательства, SHA ZIP исходников и штатного ISO.

Читай AGENTS.md, docs/PROJECT.md, docs/NEXT_CHAT_817.md,
docs/architecture/ACPICA_HOST_REPLAY_817.md и утверждённые макеты.

817 — ACPICA AcpiExec на отдельном хостовом испытательном стенде,
воспроизведение настоящих AML из QEMU/OVMF. Проверка синтетических
методов DSDT+SSDT показывает исполнение известного кода, но это НЕ
интеграция ACPICA в ядро StyleOS. Нет подтверждения физического
_CRS/_SEG/_BBN, ECAM, IOMMU или DMA. Все DMA-разрешения запрещены.

Фундамент НЕ завершён. Дальше отдельный этап интеграции ACPICA OS
services в freestanding ядро, ограничение OperationRegion, полноценная
проверка _CRS/_SEG/_BBN, ECAM, IOMMU owners/aliases/exceptions, настоящие
DMA deny/allow/fault/invalidation, UEFI runtime, production CR3 и
аппаратная интеграция обычного ядра с Desktop, процессами, .tea, StyleFS.
Только после неё сообщить владельцу, что фундамент готов.

После — «Процессы» → независимые .tea → пользователи и Welcome/Lock →
StyleFS/Проводник с «Быстрым доступом» → установщик, App World, интернет,
браузер, звук и VPN.

Не изменять main, принятые ветки, существующий Desktop, палитру,
утверждённые макеты. Каждый этап — новая ветка, полный исходный ZIP
успешного CI, ISO, UART/JSON, SHA, отчёт, точка продолжения, сообщение.
