# ADR-0009: два кооперативных контекста доверенных задач ядра

Статус: узкий эксперимент T-P1.6 в `feature/styleos-t-p1-6` от
`ea37e76abb0a1b6e75c050c43e357d8c89981537`. Проверенный код базы T-P1.5 —
`a284c30abbea2999eeaef721e0bd380c582c6dd0`. Решение относится только к
отдельному `--test-tasks` профилю. Оно не утверждает промышленную task ABI,
модель ядра или поддержку пользовательских процессов.

## Задача и границы

Две доверенные функции ядра должны действительно исполняться на разных стеках,
уступить управление явным вызовом, продолжиться после него и нормально вернуть
управление диспетчеру после return. Последовательный вызов двух C-функций или
host-модель переходов этого не доказывают. Нужны реальные SP/PC, nonvolatile
integer registers и связанные с бинарником наблюдения из QEMU.

Исполнение остаётся на одном CPU с прежними привилегиями ядра и выключенными
прерываниями. x86 сохраняет inherited CR3 и GDT/IDT/CS; ARM initial EL1 и
EL2 → EL1 используют прежний startup и EL1h с MMU off/VBAR/DAIF. Новый профиль
не меняет `boot_info v1`, memory normalization, boot pool, exception paths,
paging/ownership-профили или ordinary boot. Только прежние paging-профили могут
переключать трансляцию. Нет общего PMM/heap, reclaim, IRQ/таймера, вытеснения,
SMP, user mode, syscalls, IPC, драйверов, GUI или Deep Drop.

Все девять test-флагов попарно несовместимы; проверяются все 36 пар.
`--test-tasks` использует отдельный выход `build/tasks` по умолчанию и запрещает
явный обычный корневой `build`. Только этот профиль включает task C/ASM и новые
объекты. Прежние профили не должны содержать их символы и UART-маркеры.

## Память и происхождение адресов

| Объект | Размер и выравнивание | Использование и срок жизни |
|---|---|---|
| `styleos_tasks_stack_a`, `styleos_tasks_stack_b` | По 16384 байта, 4096 | Два полных непересекающихся image-owned BSS-объекта; весь boot |
| `styleos_tasks_control` | Union ровно 4096 байт, 4096 | Два slot context, отдельный dispatcher context, metadata, snapshots и результаты; весь boot |
| Каждый `tasks_context` | 112 байт, размещение с выравниванием 16 | Двенадцать GPR, SP по offset 96, PC по offset 104; layout проверяется `_Static_assert` |
| Прежний стек ядра | 65536 байт | Диспетчер, диагностика и ранний exception path; не заменяется новым task stack |
| Прежние diagnostic, handoff, source, pool/metadata, vectors/IDT | Полные прежние extents | Сохраняют владельца и резерв; не выдаются задачам |

После успешных handoff и MEMMAP общий demo проверяет размеры, оба конца,
арифметику, alignment, полное нахождение новых объектов в загруженном RESERVED
образе и отсутствие overlap между ними и всеми живыми исключениями. До допуска
нет новых записей в task storage. Существующая очистка BSS остаётся частью
начальной подготовки C-среды.

Бинарный `tasks-site.json` независимо подтверждает весь ARM ELF/BSS/NOBITS,
identity PT_LOAD, BSS clear bounds, `__image_end` и raw Image size/content.
Для x86 PE/map проверяются полные объекты в writable zero-fill extent,
`SizeOfImage` и RVA. Runtime прибавляет **действительный Loaded Image base**
к обоим RVA; preferred ImageBase не считается адресом загрузки.

Адреса известного ARM Image опираются на проверенные VMA=LMA, fixed header и
фактический MMU-off режим. На x86 observer доказывает identity полного текущего
образа по активному root. Это ограниченные доказательства собственных объектов,
не общее правило «C pointer равен PA» и не общий VA→PA интерфейс.

На каждом task stack крайние 16 байт снизу и сверху заняты software canary.
Полезный полуинтервал — `[start+16,end-16)`. Canary проверяется до PASS; это
наблюдение о сохранности выбранных байтов, без guard page, аппаратной остановки
overflow или защиты одного task от другого. Оба стека остаются зарезервированными
после release. Boot pool присутствует, но не допускается и не выдаёт страницы;
общая CANDIDATE/REVIEW RAM не читается, не записывается и не передаётся.
Смысл `GRANTED=0` из ADR-0008 сохраняется.

## Явный lifecycle

`kernel/tasks.c` — чистый C-движок учёта, без ASM execution, записи стеков,
вызова entry или allocation. `tasks_init()` обнуляет control и устанавливает
`current=-1`, то есть текущей стороной считается диспетчер.

| Операция | Условие | Результат |
|---|---|---|
| create | Диспетчер, FREE slot, nonnull entry, ровно 16384 байта, оба конца 4-КиБ aligned | FREE → READY, сохранение entry/full stack; context пока нулевой |
| go | Диспетчер, найден READY от round-robin cursor | READY → RUNNING, current=id, следующий cursor, dispatches+1 |
| mark_yield | current указывает на единственный RUNNING | RUNNING → READY, current=-1, yields+1 |
| mark_done | current указывает на единственный RUNNING | RUNNING → DONE, current=-1, completed+1 |
| release | Диспетчер, указанный DONE | DONE → FREE, очистка всего context/metadata слота, без обращения к stack RAM |

Если READY нет, go возвращает EMPTY и сохраняет control/output. DONE никогда
не выбирается и не становится FREE автоматически. Его stack interval остаётся
занятым при проверке overlap до явного release. Третье создание без свободного
слота отвергается. После release можно повторно использовать слот и тот же
статический stack interval; это изменение учёта, без free общей памяти.

Перед каждой мутацией проверяются все состояния/живые entry/stack extents,
непересечение non-FREE стеков, пустые metadata FREE-слотов, cursor и согласие
`current` с единственным RUNNING. Неверный index не разыменовывается. Null или
невыравненный control/output, output alias внутрь control, malformed range,
overflow, overlap, неверный переход и переполнение увеличиваемого счётчика
дают явный отказ без изменения любого байта control, другого live slot или
output id. Проверка диапазонов здесь числовая; допуск физического размещения
остаётся обязанностью demo/архитектурного слоя.

## Реальный context на границе ABI-вызова

Это экспериментальная **integer-only call-boundary ABI**, а не произвольный
снимок прерванного CPU. Caller-clobbered registers и arithmetic flags могут
измениться как при обычном C-вызове. Переход производится только в явной
cooperative точке, когда вызывающий код уже соблюдает свою ABI.

### Microsoft x64

Сохраняются RBX, RBP, RDI, RSI, R12–R15 в `gpr[0..7]`; `gpr[8..11]`
обнуляются. По входу CALL return address лежит в `[RSP]`. Переключатель
сохраняет `SP=RSP+8` и `PC=[RSP]`, то есть состояние вызывающей стороны после
RET, затем загружает регистры и SP назначения и делает JMP к его PC. Исходный
return slot больше не нужен для этого возврата. Red zone отсутствует.

Для первой выдачи usable top равен `stack.end-16`; он кратен 16. Начальный
SP равен `usable_top-40`, поэтому `SP % 16 == 8`, как на входе обычной
Microsoft x64 функции. По SP записан synthetic return address на
`styleos_tasks_trap_return`, за ним находятся 32 байта shadow space. PC —
`styleos_tasks_bootstrap`. JMP вместо CALL входит в уже подготовленный кадр.
Trap после неожиданного RET получает aligned RSP и резервирует собственные
32 байта shadow space перед вызовом диагностической C-функции.

Probe сохраняет восемь исходных nonvolatile registers через PUSH и резервирует
72 байта: 32 для shadow space, остальные для сохранённых аргументов и call
alignment. Finish резервирует 40 байт перед своим CALL переключателя.
Начальные и повторные входы соблюдают одну и ту же ABI, без общего стека A/B.

Microsoft x64 обычно включает nonvolatile XMM6–XMM15; этот опыт их не сохраняет.
Он допустим только для связанного integer-only профиля с доказанным отсутствием
FP/SIMD use. Общая совместимость с произвольной Windows x64 функцией не заявлена.

### AAPCS64

Сохраняются X19–X30, SP и PC из X30/LR. BL устанавливает LR на продолжение;
после восстановления нужной стороны BR к PC возвращает её к инструкции после
BL. SP во всех кадрах кратен 16. Probe резервирует 128 байт для исходных
X19–X30 и аргументов, finish — 16 байт перед BL.

Начальный SP равен aligned usable top. PC указывает на bootstrap, X30 — на
`styleos_tasks_trap_return`. Нет synthetic return slot и shadow space x86.
Возврат entry к bootstrap обрабатывается обычным сгенерированным C-кодом.
FP/SIMD V8–V15 и FPCR/FPSR не сохраняются: `-mgeneral-regs-only` и полный
linked decode ограничивают фактически исполняемый профиль. Platform X18/TLS,
unwind и произвольные сторонние ABI-зависимости не становятся per-task state.

## Yield, return и защита от повторного входа

Диспетчер работает на прежнем kernel stack. После go он вызывает ASM dispatch
wrapper, который прямым CALL/BL передаёт управление переключателю из отдельного
dispatcher context в выбранный slot. Сохранённый dispatcher PC обязан совпасть
с `styleos_tasks_dispatch_resume`, а SP — находиться в прежнем kernel stack.
Bootstrap проверяет RUNNING и
вызывает сохранённую доверенную entry. Task отмечает yield, после чего ASM
сохраняет его context и возвращает сохранённый context диспетчера. Следующий
go той же задачи продолжает именно этот вызов переключателя.

Нормальный return из entry попадает в bootstrap. Он регистрирует возврат,
делает mark_done и вызывает невозвращающийся `tasks_arch_finish`. Finish
передаёт управление диспетчеру, сохранив PC
`styleos_tasks_exit_resume`. Если DONE ошибочно выбрать, этот PC вызывает
`tasks_completed_resumed()` и явную ошибку. Отдельный initial return trap
обнаруживает return самого bootstrap. Обычный путь не возвращается к
завершённым инструкциям; исключения и повреждение доверенного kernel task
не восстанавливаются.

## Доказательство registers, стека и продолжения

У каждого task есть volatile local cookie с собственным значением. На START,
RESUME и EXIT проверяются то же значение/адрес и попадание local в собственный
usable stack, вне dispatcher/другого task stack. Отдельный dispatcher cookie
переживает все шесть передач на прежнем kernel stack.

ASM probe сначала сохраняет настоящие caller values на task stack, затем
заполняет nonvolatile registers шаблоном
`0x1122000000000000 + (index << 32) + tag`. Tags `0x101/0x102` для A и
`0x201/0x202` для B различают задачи и две точки yield. Прямой CALL/BL
`tasks_arch_switch` сохраняет эти реальные registers. Диспетчер печатает
сохранённые значения, SP и PC. После настоящего resume ASM сравнивает реальные
регистры до восстановления исходных caller values и возвращает C-коду mask.
Полная mask x86 — `0xff` для восьми GPR; ARM — `0x7ff` для X19–X29, а X30
соответствует связанному resume PC. Проверка копий в C-структуре не подменяет
проверку live registers после машинного перехода.

Binary proof связывает bootstrap/task entry, `tasks_arch_switch`, dispatcher
resume, probe resume, exit resume и return trap с реальными executable instruction boundaries
и bytes. Saved SP обязан лежать внутри своего доказанного usable stack,
saved yield PC — совпадать с probe resume, saved completion PC — с exit
resume. Для PE учитывается runtime relocation обоих адресов.

Требуется ровно `A:START → B:START → A:RESUME → B:RESUME → A:EXIT → B:EXIT`:
шесть GO, по два yield и successful register check каждой задачи, оба
entry-return/DONE, диспетчер EMPTY, release и сохранные canaries. После
значимых before/after architecture checks разрешены ровно один
`STYLEOS TASKS PASS` и прежний `STYLEOS KERNEL HALT`.

## Integer-only proof связанного кода

Прежние build flags x86 — `-mno-red-zone -mno-sse -mno-mmx -msoft-float`,
ARM — `-mgeneral-regs-only` с прежними strict alignment/no outline atomics.
В task-профиле недостаточно прочитать флаги: `tools/integer_code.py` требует
настоящий LLVM objdump, декодирует все доказанные file-backed executable spans
связанного PE/ELF и проверяет непрерывное покрытие исходных байтов. FP/SIMD,
неизвестный/неподдержанный opcode, неверная instruction boundary, gap, overlap
или несовпадение decoded bytes дают отказ.

В ARM Linux Image header первая 4-байтная branch исполняется и включена в
decode, а следующие 60 байт `[Image+4,Image+64)` являются объявленными header
data и исключаются. Остальной executable extent декодируется полностью,
включая padding. Proof содержит реальные compiler flags, decoder version и
команды, spans, byte/instruction counts и SHA. Sidecar и manifest повторно
проверяются на текущих бинарниках перед UART validation.

`LLVM_OBJDUMP` может задавать установленный executable; без него или подходящей
автоматически найденной LLVM-программы task proof не публикуется. Новая
зависимость не нужна старым профилям. Termux может потребовать пакет `llvm`.
Этот proof ограничивает конкретную сборку; он не реализует общий FP/SIMD
context, lazy FP, unwind, exception recovery или preemptive context.

## Read-only архитектурный observer

x86 проверяет фактические CR0/CR3/CR4/EFER/PAT, PA-width, IF/DF, GDTR/IDTR/CS,
весь Loaded Image identity, writable task storage и executable IDT targets.
До первого чтения каждая посещаемая table page целиком должна быть baseline
RESERVED и вне CANDIDATE; GDT получает отдельный реальный VA→PA proof и
полный PA-резерв. Bounded identity closure посещённых таблиц сохраняет узкую
UEFI/OVMF предпосылку прежних ADR-0007/0008. Оно не перечисляет все firmware
зависимости и не предоставляет произвольный physical access. ПО не пишет
таблицы/GDT или Accessed-биты; аппаратные A/D могут меняться при чтении.

ARM проверяет фактические EL1/SPSel/SP/DAIF/SCTLR/VBAR, fixed Image header,
полный image extent и прежний handoff. `root=0` обозначает MMU-off фазу и не
означает чтение либо очистку TTBR0. DTB не перечитывается и candidate RAM не
пробуется. До/после выполняется новое реальное наблюдение: на x86 сохраняются
значимые mode/root/descriptors и IF/DF, на ARM — EL1h/MMU off/VBAR/DAIF.
RFLAGS arithmetic bits и NZCV не входят в сравниваемую task ABI.

## Приёмка и предел результата

Host-тесты проверяют lifecycle, EMPTY/FULL, неправильные stacks/overlap/current,
invalid release/double yield/go while RUNNING, overflow, неизменность control
и output при отказе, сохранность других live slots и reuse только после
release. Они не выполняют аппаратное переключение.

Три QEMU jobs проверяют x86-64/OVMF, ARM initial EL1 и ARM initial EL2 → EL1.
Parser требует полный порядок и binary/manifest/UART связь; PASS-only,
пропущенный/дублированный resume, выход SP из своего stack, неверный cookie,
register mask/PC, повторный entry после exit, timeout/reset/тишина отвергаются.
После полного terminal marker harness прежние 200 мс наблюдает живой QEMU,
затем штатно останавливает его. Естественный exit не подменяет ожидаемый halt.

Прежние regressions, воспроизводимость четырёх ordinary artifacts и сравнение
всех 28 прежних profile artifacts с базой тем же toolchain обязательны.
Точные команды — [TESTING](../TESTING.md), реальные SHA, версии, количества и
UART/JSON — [отчёт T-P1.6](../test-results/t-p1-6/README.md). Ожидаемое число
проверок само не является PASS. Linux ARM64-only не является Android/Termux.
Owner-reported T-P1.5 Termux 35/35, exit 0, QEMU 11.1.3 по скриншоту из нового
задания фиксируется отдельно в новом отчёте: UART/JSON не приложены, это не
T-P1.6 и не изменение исторических evidence/checksums. T-P1.6 телефон,
VirtualBox и физические устройства остаются SKIPPED до реальных запусков.

Два task работают с привилегиями и адресным пространством ядра. Они могут
повредить друг друга ошибочной записью; canaries и return traps не обеспечивают
security isolation. Результат доказывает смену стека, продолжения и заявленных
integer registers доверенных функций. Отдельный будущий T-P1.7 должен
рассматривать сбой **пользовательского** процесса без падения ядра и второй
программы; данный опыт такого результата не устанавливает.

Публикуется только task branch; main, прежние refs, Portable-os,
`docs/styleos-readme`, история и параллельный presentation README сохраняются.
Нет merge, force push или release. После отчёта и публикации этап останавливается;
общая RAM, reclaim, собственная GDT, recovery и модель ядра остаются отдельными
решениями владельца.

Источники ABI: [Microsoft x64 calling convention](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention),
[AAPCS64](https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst).
Предыдущие контракты: [ADR-0002](0002-boot-handoff-v1.md),
[ADR-0005](0005-early-exception-diagnostics.md),
[ADR-0006](0006-image-owned-boot-pool.md),
[ADR-0007](0007-owned-translation-tables-experiment.md),
[ADR-0008](0008-physical-memory-ownership-plan.md).
