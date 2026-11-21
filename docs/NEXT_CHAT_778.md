# StyleOS — точка продолжения после 778

Репозиторий `LPYURA2/OS`.

Принятая база: **777**, ветка `feature/styleos-attested-detached-roots-777`,
коммит `080502e44d3913104a54112eef25593de1cdd848`, GitHub Actions
`44955737896` PASS.

Новая ветка: `feature/styleos-kernel-closure-audit-778`. Точный SHA
и итог CI брать из GitHub; не считать 778 принятым до QEMU/OVMF PASS.

Прочитать `AGENTS.md`, `docs/PROJECT.md`,
`docs/architecture/KERNEL_CLOSURE_AUDIT_778.md`,
`docs/architecture/ATTESTED_DETACHED_ROOTS_777.md`,
`docs/architecture/DESKTOP_FRAME_GRANTS_765.md`.

778 — **безопасная подготовка, не полный kernel root**: read-only
`kernel_closure_audit`, семь обязательных ролей kernel RX/RW,
предварительная проверка трёх detached корней, отказ активации при
неполном отображении. Host mock success и отрицательные тесты
ASAN/UBSAN/COFF; QEMU должен подтвердить, что корни 777 не удовлетворяют
условиям. Переключения CR3 на новый root в 778 **нет**.

**Задача 779:** собрать hardware-grounded kernel/device closure,
включая полный PE text/data, все стеки, GDT/IDT/TSS, trap handlers,
сериальный I/O, GOP/MMIO, PAT/cache policy. Только после доказательства
полноты и ownership допустим отдельный испытательный CR3 switch/restore
в QEMU. Не активировать экспериментальные адресные пространства
в обычном Desktop без доказательств.

Дальше: независимый запуск `.tea` рядом с Desktop, реальные PID/сессии,
полноценные окна и диспетчер «Процессы». Затем экран входа и пользователи
по утверждённым макетам, права и домашние каталоги StyleFS, финализация
Проводника. Полноценный установщик `.tea` ещё не реализован; App World
должен использовать тот же системный механизм установки.

Правила: отдельная ветка на каждый этап; `main` неизменен; сохранять
рабочий стол и принятые исходники; тесты честно разделять host/guest;
не выдумывать PID, CPU и RAM; выдавать ZIP исходников, ISO, журналы CI,
точку продолжения после успешной приёмки.
