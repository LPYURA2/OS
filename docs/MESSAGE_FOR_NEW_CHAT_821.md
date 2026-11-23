# Передача проекта StyleOS после этапа 821

Продолжаем разработку StyleOS, `LPYURA2/OS`.

Основа: принятый 820 `b4d768a522e5f5e8069c9da9f2f3d560cb16188d`,
GitHub Actions https://github.com/LPYURA2/OS/actions/runs/45245458602.
Рабочая ветка 821: `feature/styleos-acpica-core-link-821`.
Точный SHA и полный CI надо проверить перед дальнейшими изменениями.

821: официальный ACPICA 20260930 с SHA-256
`aa18901b92e30749be0edc3081c8d550c61fce4fa37546fc6a65d367a4ae71a5`.
Лицензия BSD-3-Clause OR GPL-2.0-only. Выбираем BSD-3-Clause и сохраняем
уведомления/текст лицензии при распространении бинарников.

Новый отдельный профиль собирает **весь ACPICA core** в COFF .lib и
выполняет на госте реальную функцию контрольной суммы из ACPICA на
проверенной копии DSDT после EBS. Это не AML runtime: в госте ещё нет
`AcpiEvaluateObject` или полной OSL, и не все компоненты библиотеки
исполняются. DMA и аппаратный OperationRegion всегда запрещены.

Обязательно читать AGENTS.md, docs/PROJECT.md,
docs/architecture/ACPICA_CORE_COFF_AND_FIRMWARE_CHECKSUM_821.md,
docs/NEXT_CHAT_821.md, docs/architecture/ACPICA_GUEST_TABLE_GATE_819.md
и утверждённые макеты Desktop/Explorer/Welcome/Lock.

Далее необходим **настоящий AML runtime** в отдельном диагностическом
госте, безопасный OSL с ограничениями, SSDT, реальные `_CRS`/`_SEG`/`_BBN`;
IOMMU ownership, DMA hardware deny/allow/fault/invalidation,
UEFI runtime, production CR3 и общая аппаратная приёмка штатного ядра
с Desktop, процессами, .tea и StyleFS. Только затем считаем фундамент
завершённым и переходим к «Процессам» и следующему продуктовому плану.

Не менять main, принятые ветки, Desktop, палитру, макеты, историю.
Каждый этап — отдельная ветка, полный CI, исходники, ISO, аппаратные логи,
SHA, технический отчёт, точка продолжения и сообщение новому чату.
