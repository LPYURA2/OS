# Сообщение новому чату — StyleOS 820

Продолжаем StyleOS, репозиторий LPYURA2/OS.
Принятая база 819: f0a00c7a12f9bcf9b65dea5b711b29cc9958c370.
Рабочая ветка 820: feature/styleos-acpica-upstream-abi-820.

Прочитать AGENTS.md, docs/PROJECT.md, docs/NEXT_CHAT_820.md
и docs/architecture/ACPICA_UPSTREAM_ABI_820.md. Проверить точный
финальный SHA/CI, ZIP, ISO, UART, SHA256 до объявления этапа принятым.

В 820 проверяется официальный выпуск ACPICA 20260930, исходный SHA,
настоящие ACPICA модули компилируются в x86-64 COFF, интерфейс
AcpiOs* проверяется через официальный заголовок acpi.h.
Это НЕ ACPICA, запущенная внутри гостевого ядра, и НЕ доказательство DMA.

Следующее: реальная guest-интеграция ACPICA interpreter с безопасной
OSL и выполнением AML + SSDT, ACPI PCI host bridge и ECAM, IOMMU ownership,
реальные DMA deny/allow + fault/invalidation, UEFI runtime, production CR3,
и полный аппаратный совместный запуск обычного ядра с Desktop,
Процессами, независимой .tea и StyleFS. До этого ФУНДАМЕНТ НЕ ЗАВЕРШЁН.

Далее по плану: полные Процессы, независимые .tea, пользователи,
Welcome/Lock, StyleFS/Проводник с Быстрым доступом, installer, App World,
интернет, браузер, звук, VPN.

Не менять main, принятые ветки, палитру, Desktop, макеты, Portable-os.
