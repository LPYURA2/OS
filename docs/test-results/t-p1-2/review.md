# StyleOS T-P1.2 — независимое review

Статус: интеграция проверена, итог ниже. Изначально review был read-only; после находки root отдельно разрешил рецензенту создать только `tests/test_qemu_process.py`. Только этот новый тестовый файл изменён рецензентом. Commit/push не выполнялись, main и Portable-os не менялись. Базовые инструкции AGENTS/ADR прочитаны; явная задача T-P1.2 разрешает исключения, ранее исключённые из объёма T-P1.1.

## Строгие критерии приёмки

- Обычный boot и старые negative handoff/map пути отвергают любые неожиданные exception/emergency markers, даже если позже присутствуют PASS/HALT.
- Управляемое исключение: ровно одна полная запись, правильная архитектура, BRK EC либо vector6 #UD, настоящий сохранённый PC тестовой инструкции, отдельный аварийный halt; обычного KERNEL HALT нет. Timeout/reset/тишина не заменяют эти свидетельства.
- x86 saved RIP сравнивается с runtime Loaded Image base + RVA тестовой UD2, с независимой проверкой байтов 0F0B в PE/COFF. Preferred ImageBase не является runtime адресом.
- ARM saved ELR сравнивается с адресом BRK в скомпонованном образе/ELF и байтами инструкции. Проверяются EL1 и initial EL2→EL1.
- ARM ESR/ELR/SPSR снимаются до C-кода на sync path; FAR численно печатается только при допустимом EC и действительных syndrome flags. BRK и unsupported async не получают свежий FAR/ESR по предположению. Historical 0x96000061 — decoder alignment evidence, не новый аппаратный Data Abort test.
- x86 error-code frames: hardware error vectors 8,10–14,17,21,29,30 различаются с no-error frames; #UD has_error=false. CR2 читается только для #PF14. Действующий CS для IDT извлекается из окружения и его ограничения объяснены. До C соблюдаются Microsoft x64 stack alignment и shadow space.
- Первый обработчик ставит reentry guard до форматирования; повторный отказ не запускает полный рекурсивный logger. UART polling конечен. Не обещается лог при неисправном UART или stack.
- boot_info v1, политика памяти, MMU/allocator/IRQ модель не меняются; прежние host/build/repro/normal/negative/ARM128/256/EL2 и ARM-only без OVMF сценарии сохраняются.

## Предварительная проверка ARM

Прочитаны стабильные `boot/aarch64/start.S` и `platform.c` после изменений ARM-агента. Vector table состоит из 16 ровно 128-байтных slots и выровнена по 2048, размер дополнительно проверяется assembler assertion. Sync slots захватывают ESR/ELR/SPSR/FAR до перехода в C; IRQ/FIQ/SError slots передают только slot, без чтения ESR. Терминальный путь не пытается восстанавливать GPR и возвращаться. UART готов после BSS/stack до установки VBAR. Сохранение x0 в x19 и EL2→EL1 остаются прежними. Test-only BRK label указывает на саму инструкцию и исключён из normal object по условию сборки. На этом этапе дефектов ARM assembly не обнаружено; общий decoder/guard и фактический target smoke ещё проверяются.

## Предварительная проверка common и x86

- Прочитан `kernel/exception.c`: raw ESR/ELR/SPSR и PC форматируются без разыменования сохранённых адресов; BRK FAR=absent. Abort FAR policy преднамеренно консервативна: поддерживаемые EC/FSC и FnV=0, остальные absent. Исторический 0x96000061 классифицируется как data alignment fault, FnV-вариант подавляет FAR. Это pure decoder evidence, не аппаратный Data Abort.
- Первый вход ставит volatile guard перед formatter/UART; повторный вход сразу вызывает arch_halt без UART. Асинхронный ARM path получает только slot и печатает raw syndrome fields как absent. Polling COM1/PL011 остаётся конечным. Гарантии при отказе стека/UART и асинхронном NMI/SError не заявлены.
- Независимо запущен `tests/test_exception.c` с GCC ASan+UBSan в `review-host/`: **PASS**, включая decoder, точное форматирование, аппаратные error-vector flags и first-entry/reentry.
- Дополнительный read-only x86 reviewer подтвердил аппаратные error frames, RIP+24 до realignment, CR2 только #PF14, CLD/CLI, MS ABI 16-byte alignment + 32-byte shadow + fifth argument, current-CS/GDT dependency, IDT после EBS на собственном стеке. Report: `x86-review.md`, source snapshot `x86-review-hashes.json` рядом с этим файлом. В reviewed x86 sources блокеров нет.

Окончательная проверка интеграции ещё ожидает PC proof helper и фактические exception UART logs/root --all.

## Обнаруженная находка P2: естественный выход QEMU мог давать PASS

В `Checks.run_qemu()` после получения ожидаемых UART markers не различались естественное завершение/reset процесса и остановка самим harness. Рецензент воспроизвёл на изолированном fake-QEMU: процесс печатает полностью принимаемый x86 memory/exception журнал с vector6/PC/EMERGENCY, затем вызывает `os._exit(1)`. Runner вернул PASS и сообщение «QEMU stopped by harness», хотя реальный returncode процесса = 1. Это противоречит требованию задачи «неожиданный reset — не PASS».

Доказательство: `review-host/fake-exit/result.json`. Настоящий QEMU для probe не запускался; repo не изменялся. Родительский агент уведомлён. Исправление должно учитывать факт живого процесса перед намеренной остановкой и недопустимые статусы завершения; QEMU может корректно обработать SIGTERM и возвратить 0, поэтому проверять только -SIGTERM недостаточно. Исправлено root: runner собирает журнал ещё 200 мс после полной terminal line, отвергает естественный exit/reset, проверяет факт намеренной остановки и допустимый returncode, пишет lifecycle facts в footer. Рецензент по отдельному разрешению root добавил `tests/test_qemu_process.py`: **8/8 PASS**. Это реальные изолированные fake subprocesses с замоканным только UART classifier, не QEMU boot evidence. Покрыты естественные exit0/exit1, exit0 через 50 мс после маркера, процесс до SIGTERM, корректный обработчик SIGTERM→0, ошибочный SIGTERM→1, неполная строка и незавершённое окно наблюдения. Dummy image hashes соответствуют dummy files, поэтому happy-path действительно достигает lifecycle/classifier gates. Лог: `review-host/process-tests.log`. Находка **закрыта**.

Неблокирующая терминологическая неточность EC0x00 была сообщена автору: архитектурное значение — Unknown reason, а не доказанно unknown instruction. Автор исправил строку на `unknown-reason` и добавил host assertion.


## Независимая сверка фактических PC и ограничений

Прочитаны реальные root QEMU/OVMF журналы из `build/t-p1-2/test-results/` и соответствующие бинарники/metadata:

| Путь | Подтверждение |
|---|---|
| x86 #UD | Runtime IMAGE base `0x06029000` + PE fault RVA `0x4b42` = saved RIP и test probe `0x0602db42`; сырые EFI bytes `0f0b`. Preferred ImageBase `0x140000000` отличается, то есть реальное PE relocation учтено. |
| ARM BRK EL1 | Saved PC/ELR `0x40083908` = ELF symbol; ELF и загруженный raw Image по `pc - 0x40080000` содержат `002420d4` (BRK #0x120); ESR `0xf2000120`, FAR absent. |
| ARM BRK initial EL2 | Initial EL2/current EL1 и тот же точный PC/ESR/BRK; одна полная запись и отдельный аварийный halt. |

Оба архитектурных fault журнала содержат ровно одну полную запись и один EMERGENCY HALT, обычного KERNEL HALT нет; binary/raw hashes соответствуют metadata. Lifecycle footer реальных QEMU: requested_stop=True, natural_exit=False, returncode=0, settled=True. Отсутствие обычного halt не заменяется ожиданием timeout.

В обычных ARM/x86 entry objects отсутствуют injection labels `arch_test_exception`/`styleos_test_fault`. Build flag взаимоисключён со старыми negative modes, по умолчанию создаёт отдельный `build/exception`; выбор общего default build каталога для managed fault отвергается.

Побайтно сверены с e482: boot_info header/validator; memory contract/policy/diagnostic; UEFI/FDT memory parsers; EBS helper/header; оба UART; ARM linker script. Все 12 файлов неизменны. Новые IDT/guard/vector области входят в image-owned память; старые правила кандидатов/резервов не меняются. Сырые PC/FAR не разыменовываются.

Файл `review-host/integration-evidence.json` сохраняет числовую сверку и результаты protected-file comparisons.

## Интеграционные результаты и итог

Прочитан завершённый root `build/t-p1-2/test-results/results.json`: **23/23 PASS**. Host Python **39/39 PASS**, включая восемь процессных регрессий. Сохранены прежние negative handoff/truncated map, обе normal загрузки, ARM EL2 и 256 МиБ, reproducibility всех четырёх артефактов. Новый набор выполняет actual #UD x86 и BRK ARM EL1/EL2. Также прочитан отдельный ARM-only набор `build/termux/test-results/results.json`: **16/16 PASS** (Linux с выбранным Clang, без x86/OVMF jobs). Этот результат не выдаётся за новое испытание телефона.

**Итог review: PASS в пределах T-P1.2; открытых блокирующих или P2-находок нет.** Найденный process lifecycle false-positive исправлен и закреплён регрессиями. Финальная документация, staged/privacy scan, commit/remote verification остаются задачами root.

Пределы: coverage начинается только после собственных VBAR/IDT; inherited x86 GDT сохраняется, собственных GDT/TSS/IST нет. Нет гарантии при сломанном стеке/UART, нет recovery, поддержка IRQ/NMI/SError не объявляется. Hardware Data Abort/#PF и реальный Android T-P1.2 здесь не проверялись; historical ESR подтверждён decoder test. Модель ядра, аллокатор, MMU, IRQ-контроллеры и user space не выбраны/не реализуются этим этапом.
