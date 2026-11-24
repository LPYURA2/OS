## Stage 853 — polled PS/2 and bounded cursor damage candidate

See [architecture](architecture/DESKTOP_INPUT_853.md), [handoff](NEXT_CHAT_853.md), [new chat](MESSAGE_FOR_NEW_CHAT_853.md). Terminal fixture only; acceptance requires exact successful CI and independent evidence review.

## Stage 799 — isolated downstream PCI BAR aperture measurements

Dedicated QEMU test probes and restores two selected VirtIO Serial MMIO BARs
behind a PCIe bridge, checking whole extents against forwarding windows;
never enables production CR3 or changes normal Desktop. See
[architecture](architecture/PCI_DOWNSTREAM_BAR_MEASUREMENT_799.md),
[handoff](NEXT_CHAT_799.md) and [new-chat message](MESSAGE_FOR_NEW_CHAT_799.md).

## Stage 798 — same-CI measured-versus-unmeasured PCI MMIO range census

Read-only evidence audit correlates S795's measured VGA BAR0 aperture with
independent S796 topologies and S797 route verification. It **never** treats
bridge window room as measured BAR size or authorizes production CR3. See
[architecture](architecture/PCI_MMIO_EXTENT_CENSUS_798.md),
[handoff](NEXT_CHAT_798.md), [new chat](MESSAGE_FOR_NEW_CHAT_798.md).

## Stage 794 — Q35 PCI BAR to GOP base correlation (terminal evidence)

## Stage 797 — audit PCI bridge MMIO base forwarding

На аппаратных UART двух QEMU конфигураций 796 проверяются начальные адреса downstream BAR через окна мостов (не размер!). Запрещены ложное одобрение production CR3 и выводы о полной MMIO карте. [Отчёт](architecture/PCI_MMIO_FORWARDING_BASE_AUDIT_797.md), [handoff](NEXT_CHAT_797.md).

New terminal-only PCI configuration mechanism #1 scan of PCI bus zero.
Correlates a detected display device **memory BAR base** to the observed GOP
framebuffer physical start. Does not size the BAR, prove aperture coverage,
resolve PAT/MTRR effective caching, route interrupts, authorize production CR3
or modify ordinary Desktop. Full validation requires actual CI/QEMU.
See [report](architecture/PCI_GOP_BAR_OBSERVATION_794.md),
[handoff](NEXT_CHAT_794.md) and [new chat](MESSAGE_FOR_NEW_CHAT_794.md).

## Stage 793 — UEFI device/cache observations (terminal evidence)

Stage 793 adds an independent GOP/EFI-descriptor/CPU register diagnostic and
strict guest log verifier; ordinary Desktop, boot_info v1 and production CR3
are unchanged. See [architecture](architecture/UEFI_HARDWARE_OBSERVATION_793.md),
[handoff](NEXT_CHAT_793.md), [new-chat message](MESSAGE_FOR_NEW_CHAT_793.md).
Hardware claims require exact successful CI.

## Этап 790 — собственные low-half таблицы и W^X-границы (эксперимент)

Отдельная ветка `feature/styleos-owned-low-tables-wx-790` от принятого 789.
Новый терминальный EFI `--test-owned-low-790` заменяет низкоуровневую цепочку
PDPT/PD/PT собственной и размечает загруженный PE-образ по разрешениям
RX/RO+NX/RW+NX на страницах 4 КиБ. Сохраняются собственные IST1–IST3;
Desktop не меняется. Этап принимать **только после настоящего CI/QEMU PASS**,
не путать с production W^X и совместным запуском .tea. См.
[отчёт](architecture/OWNED_LOW_PAGE_TABLES_WX_790.md),
[точку продолжения](NEXT_CHAT_790.md),
[сообщение новому чату](MESSAGE_FOR_NEW_CHAT_790.md).

## Этап 789 — ограниченный унаследованный PML4, только терминальный эксперимент

Ветка `feature/styleos-pml4-root-pruning-789`. Отдельный `--test-root-slots-789`
удаляет все унаследованные PML4-слоты за исключением низкого slot 0 и
отдельного собственного slot 128 для IST, проверяет настоящий INT3/#PF/#DF
в QEMU. Таблицы ниже PML4[0] остаются унаследованными. Не работает в
обычном Desktop и **не** доказывает полное W^X. Принятие только по CI точного
коммита. [Отчёт](architecture/TERMINAL_RESTRICTED_PML4_789.md),
[продолжение](NEXT_CHAT_789.md), [сообщение](MESSAGE_FOR_NEW_CHAT_789.md).

## Stage 787 — guard page #PF on separate IST2 (experimental)

Task branch `feature/styleos-hardware-guard-pf-787` starts from accepted 786.
Only isolated terminal `--test-guard-pf-787` may raise the real guard-page
write page-fault. Does not affect ordinary Desktop. Full acceptance requires
actual GitHub QEMU/OVMF `#PF` evidence, checked final CI and archive, NOT
host software checks. See [787 architecture](architecture/HARDWARE_GUARD_PF_IST2_787.md)
and [handoff](NEXT_CHAT_787.md). #DF/stack exhaustion remain unproved.

## Этап 786 — аппаратный IST1 в отдельном терминальном QEMU

Ветка `feature/styleos-hardware-ist-entry-786` от принятой 785. Отдельный
`--test-ist-hw-786` включает унаследованный клонированный CR3, NX supervisor
IST1 с двумя нулевыми guard PTE, свежие GDT/TSS/IDT, реальный `INT3`
и безвозвратный выход из VM. Не подключено к обычному Desktop; результат
доказывается только финальным GitHub Actions. Не доказаны переполнение
стека, guard-page #PF, двойной fault, production memory closure и совместный
Desktop + независимый .tea. [Отчёт](architecture/HARDWARE_IST_GUARD_ENTRY_786.md),
[точка продолжения](NEXT_CHAT_786.md),
[сообщение для нового чата](MESSAGE_FOR_NEW_CHAT_786.md).

## Этап 785 — неактивные guard-таблицы для будущего IST

Отдельная ветка `feature/styleos-ist-guard-table-plan-785` от принятого 784.
Неактивная 4-уровневая схема с одной NX supervisor IST1-страницей и двумя
отсутствующими PTE; программные ASAN/UBSAN и COFF тесты. Не переключает CR3,
не активирует IDT/TSS/IST, не меняет обычный Desktop. Аппаратная проверка
IST/guard — следующая отдельная стадия. [Отчёт](architecture/INACTIVE_IST_GUARD_TABLES_785.md),
[точка продолжения](NEXT_CHAT_785.md), [сообщение новому чату](MESSAGE_FOR_NEXT_CHAT_785.md).

## Stage 784 — isolated terminal GDT/TSS install test (candidate)

Branch `feature/styleos-tss-lgdt-ltr-trial-784` from the exact accepted
783 SHA. Only a separate QEMU-only test image can execute LGDT and LTR,
read STR and the busy bit, then exit the entire VM without firmware return.
The ordinary Desktop and 783 frame-grant profile are unchanged. **No IST,
guard-PTE or production TSS claim.** [784 architectural report](architecture/TERMINAL_GDT_TSS_LTR_784.md)
and [next-chat handoff](NEXT_CHAT_784.md). Final success requires the exact
SHA and complete CI run; this text alone is not acceptance.

## Stage 780 — live PE W^X intent and missing kernel dependencies

Branch `feature/styleos-kernel-dependency-census-780` from accepted 779.
Read-only test records actual loaded PE section roles and unresolved hardware
requirements, including TSS/IST, device MMIO, firmware runtime and cache
policy. **It never grants a CR3 switch**; see
[the 780 report](architecture/KERNEL_DEPENDENCY_CENSUS_780.md) and
[handoff](NEXT_CHAT_780.md). Verify final CI by exact commit.

## Этап 777 — проверенные физические страницы и отсоединённые корни

Ветка `feature/styleos-attested-detached-roots-777` объединяет 765
hardware frame proof и 762/764 detached safe/user roots. Требует
подтверждения всех 79 страниц, инициализирует три корня без включения
их в штатную загрузку. Это НЕ завершённый PMM, kernel closure или
графический запуск .tea. Статус CI только по финальному коммиту.
См. [отчёт 777](architecture/ATTESTED_DETACHED_ROOTS_777.md) и
[точку продолжения](NEXT_CHAT_777.md).

## Этап 775 — связь транзакции процессов с оконным IPC

Новая ветка `feature/styleos-native-desktop-launch-775` добавляет отдельный
kernel-only координатор процесса: реальный PID, подготовленные страницы,
сеанс, окно и IPC-порт принимаются как одна транзакция **до исполнения**
пользовательского кода. При отказе выдачи порта готовый к запуску PID
завершается с отзывом страниц. Принятие требует зелёного полного CI
и проверки SHA. Это ещё **не** запуск графического `.tea` рядом с Desktop.
См. [обоснование](architecture/NATIVE_DESKTOP_ADMISSION_775.md) и
[продолжение](NEXT_CHAT_775.md).

## StyleOS 772 — isolated x86 CPL3 kernel user-copy boundary

Task branch: `feature/styleos-cpl3-user-copy-772`. Base accepted: 771 (`96834d3b`).
Two dedicated QEMU profiles exercise actual CPL3 INT80 trap copying a private
user stack word into trusted image memory and refusal of a foreign B page.
The normal Desktop stays unchanged, including its zero-port window channel.
The new copy-only trap is **not yet production syscalls or `.tea` IPC/rendering**.
See [`CPL3_USER_COPY_BOUNDARY_772.md`](architecture/CPL3_USER_COPY_BOUNDARY_772.md)
and [`NEXT_CHAT_772.md`](NEXT_CHAT_772.md). Confirm final CI before acceptance.

## 771 — kernel-owned канал будущих окон `.tea` (21 ноября 2026)

В ветке `feature/styleos-desktop-window-ipc-771` добавлена в обычную загрузку
x86-64 пустая служба ограниченного обмена с окнами нативных приложений.
Модель проверяет настоящий PID, root, аппаратный handle, активный сеанс и
владельца окна; копирует ограниченные события и маленькую плитку пикселей,
сохраняет state при рестарте композиции, отзывает окна умерших процессов.
Это **не user-mode syscall, не GUI окно и не запуск `.tea` в Desktop**.
История/ограничения: [DESKTOP_WINDOW_IPC_771](architecture/DESKTOP_WINDOW_IPC_771.md).
CI соответствует только точному SHA коммита в ветке.

## Этап 768 — аппаратный ELF64 из .tea

Новая ветка `feature/styleos-native-tea-cpl3-768` от принятого 767. Добавлен
отдельный экспериментальный x86-64 профиль: compile ELF64 → canonical `.tea`
→ host verified/extracted ELF → ограниченный freestanding loader → BSS user RX
→ реальный CPL3 с PID/YIELD/EXIT. Это **не** runtime `.tea` загрузчик рабочего
стола, нет доступа к файловой системе гостя, пользовательских окон/сеансов,
динамической линковки и системного IPC. Отдельное аппаратное доказательство
и результат CI см. в [отчёте 768](architecture/NATIVE_TEA_ELF_CPL3_768.md).

# StyleOS — точка входа в проект

Что строим, что подтверждено и что делать дальше.

## Этап 765 — image-backed page attestation (отдельная ветка)

- Проверенная основа: `feature/styleos-frame-ownership-764`, коммит
  `f8d38cea0aca9b5b0cf24da275a45f1d0e5ad39d`, CI PASS.
- Задача ветки `feature/styleos-physical-grant-attestation-765` —
  подтверждение физических адресов строго 72 табличных и 6 пользовательских
  страниц **внутри тестового PE-образа** по карте UEFI, полному read-only
  old-CR3 walk, PE symbol RVAs и негативным тестам. Новые таблицы не
  активируются, общая RAM не выдаётся, PMM не создан.
- Реальный CI и ограничения описаны в
  [архитектурном отчёте](architecture/DESKTOP_FRAME_GRANTS_765.md).
- Продолжение — [после этапа 765](NEXT_CHAT_765.md). Точный результат
  текущей ветки смотреть в GitHub Actions; непрошедший этап не принимать.

## 766 — ядро процесса/сеанса (21 ноября 2026)

Новая независимая ветка `feature/styleos-user-session-boundary-766` отделяет
kernel PID и жизненный цикл нативного `.tea` от исторических UI app session
ID рабочего стола. Добавлен kernel-only runtime для привязки настоящих PID к
сессиям будущих пользователей, выбора/блокировки сеанса, фильтрации окон и
проверки родителя; **аккаунтов, паролей, файловых ACL или экранов входа ещё
нет**. Обычный рабочий стол сохраняется; служба загружается пустой. Документ:
[USER_SESSION_PROCESS_BOUNDARY_766](architecture/USER_SESSION_PROCESS_BOUNDARY_766.md).

## Актуальный восстановительный контур — 21 ноября 2026 года

После частичной потери поздних исходников за базу восстановления взяты
сохранённые исходники 709 и контрольный двоичный образ 730. Состояние
настольного приложения и состояние системного ядра проверяются раздельно:
прежние пройденные тесты не доказывают, что все функции утраченных сборок
восстановлены побайтно.

- **Этап 759**, `3ffe22b7`: модель защищённого снимка процессов — CI PASS.
- **Этап 760**, `a91c9424`: два тестовых аппаратных пользовательских
  контекста с отдельными PID и учётом событий CPL3/EL0 — CI PASS.
- **Этап 761**, ветка `feature/styleos-desktop-process-bootstrap-761`:
  интеграция ядра диспетчеризации в обычный десктоп; до независимого `.tea`
  и полноценных syscall/запуска приложений это **нулевой реестр процессов**.
  Точный результат нового CI см. в GitHub Actions ветки. Инженерные условия
  и ограничения описаны в [отчёте этапа 761](test-results/desktop-process-runtime-761/README.md).
- **Этап 762**, ветка `feature/styleos-owned-vm-stage-762`: отдельный
  неактивируемый конструктор конечных таблиц x86-64 (SAFE и два
  пользовательских адресных пространства), проверки W^X, границ,
  физической изоляции и отказов. Без перехода CPL3 в обычном рабочем столе.
  [Ограничения и тесты](architecture/DESKTOP_VM_STAGE_762.md).
- **Этап 763**: изолированный QEMU-тест x86-64 с реальным CR3 roundtrip
  на старых BSS-таблицах T-P1.4 — CI PASS на `e7258f1`.
  Это **не** активация 762 корней.
- **Этап 764**, ветка `feature/styleos-frame-ownership-764`: первый конечный
  реестр физ. грантов с защитой от перекрёстной выдачи и PID-ориентированным
  отзывом таблиц. Он сам не выдаёт и не доказывает физические гранты; см.
  [ограничения](architecture/DESKTOP_FRAME_OWNERSHIP_764.md).
  Результат CI определяется по точному SHA ветки.
- [Точка продолжения для нового чата](NEXT_CHAT_764.md).
- `main` и старые ветки намеренно не заменяются экспериментальным кодом.
  Существующий рабочий стол и палитра сохранены.

Ниже — **исторический срез 18 ноября**, оставленный без переписывания задним
числом. Его ссылки на 703/SDK отражают состояние на ту дату, а не 761.

## Продолжение: 767 — транзакция запуска независимого приложения

Этап 767 находится в ветке `feature/styleos-native-launch-transaction-767`.
Подготовлен **kernel-only** контракт: привязка подготовленного CPL3
контекста к реальному PID, отдельному владельцу страниц, пользовательскому
сеансу и окну; при сбое — завершение, удаление окон/привязки и fail-closed
отзыв root. Это не реализация ELF загрузчика и не означает, что сейчас
обычный рабочий стол исполняет отдельные `.tea` процессы. Подробности:
[транзакция native launch 767](architecture/NATIVE_LAUNCH_TRANSACTION_767.md),
[точка продолжения](NEXT_CHAT_767.md).

## Настольный контур на 18 ноября 2026 года

- **Принятая сборка для ВМ — 703**, ветка `feature/styleos-explorer-v2`
  на `bce3eaf`. Владелец запустил тестовую сборку в VirtualBox и
  подтвердил работу. Системный VDI и отдельный VDI данных уже используются
  при офлайн-обновлении. Это не подтверждение установки на физический ПК.
- Следующий инкремент — отдельная папка «Обновления», привычный двойной
  щелчок и переключение RU/EN. Затем «Заметки», «Документы» с первым
  просмотром `.docx`, Deep Drop и установка внешних `.tea`.
  [Текущий порядок](ROADMAP.md#ближайшие-настольные-этапы)
- Ниже сохранён инженерный срез для ядра от 17 ноября. Его статусы
  не описывают более новые настольные сборки.

## Куда идём

StyleOS — самостоятельная ОС с одним рабочим пространством на ПК и телефоне с монитором, собственным контролем над файлами, задачами и обновлениями. Ближайшая практическая цель — рабочий VirtualBox-образ, в котором можно написать, сохранить и заново открыть документ. Следующий результат — установка внешнего приложения в ту же систему и перенос выбранного объекта через Deep Drop. [Критерии первого инженерного выпуска](ROADMAP.md)

## Что подтверждено

| Точка | Состояние |
|---|---|
| Актуальная проверенная ветка кода | [feature/styleos-desktop-first-image](https://github.com/LPYURA2/OS/tree/c162995c4390686bda54468f68f40164b3c4f3bc), `c162995`. На ней оба исправления Termux и отдельный графический тест. |
| Принятая база ядра | [`40c3987`](https://github.com/LPYURA2/OS/commit/40c3987d4b78e0f67888db8d6baa24c7af4a35d0). T‑P1.8B в Termux — 16/16, код 0. |
| Linux/QEMU на актуальной ветке | 57/57 полных регрессий, 16/16 disk-app, 2/2 графических сценария, 48/48 прежних артефактов совпали с базой. |
| Android/Termux, x86‑64 в QEMU | Оба диагностических VDI загрузились по COM1. У графического есть DISPLAY PASS. Кадр на телефоне отдельно не подтверждён. |
| VirtualBox и нативное оборудование | Для этих VDI ещё не проверены. |
| `main` и выпуск | Ветка по умолчанию содержит кодовую базу `c162995` с актуальной документацией. Проверенный код не менялся при переносе. T‑P1.0 сохранена в `legacy/styleos-t-p1-0` на `dfc5be9`. Публичного выпуска на дату среза нет. |
| SDK, .tea и Deep Drop | На дату среза опубликованная ветка SDK не обнаружена. Ближайшая задача — внешний пример «Презентации», его сборка и проверяемый пакет .tea. Хостовая проверка не равна установке в госте. |

[Графический отчёт Linux](test-results/desktop-display/README.md) · [Контроль владельца и независимый аудит](test-results/owner-2026-11-17.md) · [Все ветки и исторические результаты](BRANCHES.md)

## С чего продолжать

- **SDK** — сначала небольшой самостоятельный шаг. Стандартным Clang собрать минимальный внешний пример, упаковать в .tea и прогнать положительную и отрицательную проверки пакета. Deep Drop, живую установку и GUI не считать выполненными в этой задаче.
- **VirtualBox** — отдельно запустить два уже проверенных диагностических образа в одной x86‑64 UEFI VM по очереди. Для базового требуется COM1, для графического также экран STYLEOS / DISPLAY CHECK. [Инструкция](QUICKSTART.md)
- **Оболочка и Deep Drop** — не потеряны. Приёмка, порядок зависимостей и открытые вопросы есть в [дорожной карте](ROADMAP.md).

Для нового рабочего чата: «Прочитай этот файл, [правила репозитория](../AGENTS.md) и связанный отчёт. Сверь актуальный ref и SHA. Продолжай только согласованный шаг. Если найден более новый отчёт, не подменяй им статус другой среды».

## Карта документов

| Что нужно | Где читать |
|---|---|
| Объяснение проекта для посетителя | [README](../README.md) |
| Итоговые цели и последовательность | [Дорожная карта](ROADMAP.md) |
| Что согласовано, что открыто и почему | [Реестр решений](DECISIONS.md), [технические ADR](adr/) |
| Идеи, не входящие в текущие задания | [Идеи](IDEAS.md) |
| Что менялось и проверялось по веткам | [Указатель веток](BRANCHES.md) |
| Как ведём этап от идеи до релиза | [Порядок работы](WORKFLOW.md) |
| Команды, результаты, журналы и границы | [С чего начать тестирование](QUICKSTART.md), [TESTING](TESTING.md), [отчёты](test-results/) |
| История и устройство кода | [История](HISTORY.md), [карта кода](LEARNING_RU.md) |

**Примечание об истории.** Старые README, ADR и отчёты описывают свой срез. Если отчёт исторически отмечает Termux как SKIPPED, а позднее владелец принёс успешный вывод, оба факта остаются в документации. Не переписываем старый отчёт так, будто результат был известен на дату его публикации.

## Этап 802 — согласование PAT/MTRR/PTE из независимых QEMU-запусков

Новый обязательный CI объединяет **свежие аппаратные логи 800 и 801** и
отклоняет несогласованные CR3/PAT/физические адреса. Для трёх сэмплов
MTRR UC и PAT WB согласуются с UC, но не заменяют глобальное сканирование
алиасов или hardware same-boot proof. Production CR3 остаётся запрещённым.
См. [архитектуру](architecture/CROSS_BOOT_CACHE_RECONCILIATION_802.md),
[точку продолжения](NEXT_CHAT_802.md) и [сообщение](MESSAGE_FOR_NEW_CHAT_802.md).

## 803 one-boot cache evidence

Terminal QEMU-only PAT/MTRR/PTE sample, same-boot, no global alias proof or production CR3. See `docs/architecture/SAME_BOOT_CACHE_SAMPLE_803.md`.

## Stage 804 — test-only complete-known-MMIO-extent audit

S804 expands S803 single-address cache samples to all 1005 physical 4KiB pages
of three fixed-QEMU MMIO ranges (GOP pixels, VirtIO Serial BAR1/BAR4).
Independent UART and host verifiers enforce gapless extent coverage; **no**
production CR3, global alias proof, other-device policy or normal desktop
modification is implied. Details: `docs/architecture/MMIO_EXTENT_PAGE_AUDIT_804.md`.

## Stage 805 (diagnostic task branch)

Read-only, dedicated QEMU/OVMF post-EBS IRQ/APIC CPU snapshot. See
`docs/architecture/IRQ_APIC_CPU_SNAPSHOT_805.md`. This does **not**
activate APIC routing in native Desktop or authorize production CR3.

### Stage 806 — ACPI MADT/IOAPIC read-only topology
A separate terminal diagnostic observes trusted QEMU/OVMF RSDP/XSDT/MADT without changing standard Desktop. Only topology and checksums; IRQ routing and delivery unverified. See `docs/architecture/ACPI_MADT_IOAPIC_TOPOLOGY_806.md`.

## Утверждённый владельцем порядок дальнейшей разработки (24.11.2026)

1. Завершение фундамента системы: память, защитные механизмы, аппаратные зависимости, прерывания, изоляция и проверка безопасного перехода на собственные таблицы. Диагностические успехи не означают готовность production.
2. Файловая система: сохранность, восстановление, права доступа, совместимость данных при обновлении.
3. Независимые родные приложения .tea и их системные контракты.
4. Проводник.
5. Экран приветствия, входа, выбора пользователя и блокировки.

Интерфейс строго по утверждённым макетам (docs/design/APPROVED_UI_REFERENCES.md); существующий Desktop не изменять в фундаментальных этапах. Порядок выше заменяет прежнюю очередность, но не отменяет уже принятые результаты. Каждый новый этап в отдельной ветке, main и принятые refs неизменны.

Контроль завершения текущего фундамента: [FOUNDATION_COMPLETION_GATE.md](architecture/FOUNDATION_COMPLETION_GATE.md). Это инженерные критерии, не заявление о готовности. В каждом отчёте явно сообщать текущий статус фундамента.

Решение владельца 24.11.2026: после завершения реализации фундамента — отдельная комплексная приёмка всего ядра по [KERNEL_FINAL_ACCEPTANCE.md](architecture/KERNEL_FINAL_ACCEPTANCE.md). До её PASS фундамент окончательно не принят; файловую систему не начинать.

После успешной комплексной приёмки ядра и до файловой системы — отдельное обсуждение владельцем [технического уровня, оптимизации, производительности и системных требований](architecture/POST_ACCEPTANCE_PERFORMANCE_REVIEW.md). Числа не утверждены; решение об объёме дальнейшей оптимизации принимает владелец.
