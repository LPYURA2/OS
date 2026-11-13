# StyleOS T-P1.0 — независимое инженерное review

Дата: 13.11.2026. Область: boot_info, общий C-код, UEFI/EBS/ABI/стек, AArch64 entry/EL/DTB, сборка, GPT/FAT16, test runner и исходники перед публикацией. Репозиторий рецензентом не редактировался; commit/push и изменения Portable-os не выполнялись. Тяжёлый QEMU не дублировался — прочитаны реальные журналы полного прогона root.

## Находки

### P2 — normal smoke пропускал x86 BOOT_ERROR в журнале — ИСПРАВЛЕНО

`tools/test.py`, конец `Checks.run_qemu()`: отрицательная проверка ищет `b'BOOT ERROR'`, тогда как `boot/x86_64/uefi.c` диагностирует ошибки префиксом `STYLEOS BOOT_ERROR` с подчёркиванием. Поэтому журнал с ошибкой UEFI и последующими положительными маркерами может получить PASS, вопреки описанному правилу «boot error = неуспех».

Воспроизведено отдельным fake-QEMU процессом, который печатает:

```text
STYLEOS BOOT_ERROR invalid UEFI system table
STYLEOS HANDOFF PASS x86_64
STYLEOS KERNEL HALT
```

Runner вернул PASS. Доказательство: `review-host/fake-qemu/result.json` рядом с отчётом. Этот probe не является результатом загрузки настоящей ОС и не подменяет реальные QEMU-журналы.

Исправлено родительским агентом: `serial_handoff_ok()` отклоняет `BOOT_ERROR`, `BOOT ERROR` и `early exception`; normal/negative/ARM EL условия вынесены в единую проверку. Добавлены четыре регрессионных теста `tests/test_qemu_output.py`. Рецензент независимо запустил их: **4/4 PASS**; тот же fake-QEMU payload после правки теперь получает **FAIL**, как требуется. Доказательства: `review-host/serial-regression.log`, `review-host/fake-qemu/result-after-fix.json`. Находка закрыта.

Блокирующих дефектов самого машинного старта в рассмотренном объёме не обнаружено.

## Подтверждённые свойства кода

- **boot_info:** фиксированная раскладка 96 байт, size/version/magic и архитектурные варианты сверены. UEFI карта проверяется на положительную длину, шаг/версию, кратность и переполнение диапазона; физические адреса не выдаются за доказанно безопасные произвольные указатели. Резерв и несовместимые поля отвергаются.
- **UEFI ABI:** минимальные структуры EFI имеют корректные x64 offsets; callbacks используют Microsoft x64 ABI. Целевая сборка `x86_64-pc-win32-coff` применяет тот же ABI к C-ядру. Assembly сохраняет RCX handoff, заменяет RSP, выравнивает по 16 и резервирует 32-байтное shadow space перед call.
- **UEFI/EBS:** буфер карты выделяется как LoaderData, сохраняется после успеха; между последним GetMemoryMap и EBS только записи в info. Устаревший ключ ограниченно повторяется; growth/allocation/error paths конечны. После успешного EBS нет Boot Services вызовов. Ошибка EBS терминальна; firmware console не вызывается после попытки выхода. Отдельный image-owned stack и сам PE-образ остаются живыми.
- **Формат x86:** `objdump -x` подтвердил PE32+ AMD64 EFI application, нулевой timestamp, `.reloc`, отсутствие imports. Дизассемблированный handoff соответствует исходному stack/ABI-коду. BSS-стек входит в виртуальный размер PE-образа.
- **AArch64:** ARM64 Image header, адрес `0x40080000`, сохранение x0/DTB в x19, EL2→EL1h с masked DAIF, EL1 path, 16-байтный стек, очищение BSS 8-байтными stores и VBAR_EL1 согласованы с linker script. ELF segments/symbols подтверждают границы; `__image_end` включает BSS/стек. Используется ограниченный QEMU virt boot contract с MMU-off окружением, не универсальный firmware entry.
- **DTB:** код проверяет только поля фиксированного FDT header, не всё дерево, резервирования или доступность произвольного physical pointer. Это явно оговорено в коде/ADR/README. PL011 фиксирован для QEMU virt; полноценное обнаружение устройств отсутствует.
- **Остановка:** x86 CLI/HLT loop, ARM masked DAIF/WFE loop не возвращаются в завершённое boot environment. Serial markers и завершение процесса runner'ом не являются аппаратным измерением состояния каждого CPU; inspected halt-код и boot-smoke дают разные части доказательства.
- **Сборка:** исходники фиксированы списками, subprocess без shell, реальные freestanding targets и LLD, известные выходные пути. Нормальные и отрицательные сборки находятся в разных каталогах. Повторяемость сравнивает четыре артефакта и пересчитывает их SHA-256; stale manifest не превращает неуспешную сборку в PASS.
- **Диск:** геометрия GPT/backup arrays/CRC и FAT16 cluster range согласованы. Две directory clusters и file chain помещаются в FAT и ESP; payload читается ограниченно, размер контролируется. Прототип хранит только EFI boot image и не содержит пользовательского диска. Проверена узкая OVMF/QEMU цель; FAT16 ESP не является обещанием совместимости со всеми firmware.
- **Безопасность записи:** make_disk отклоняет block/nonregular destination до open, проверяет source/destination alias, создаёт tempfile в destination directory и публикует os.replace. Пишется только явно объявленный output. QEMU монтирует создаваемый диск read-only и использует отдельную VARS-копию; исходный firmware шаблон не изменяется.

## Дополнительные реальные проверки рецензента

Все нижеперечисленные проверки выполнены в `/workspace/scratch/styleos-t-p1-0/review-host`, без изменений исходников:

| Проверка | Результат |
|---|---|
| Existing boot_info host tests, GCC ASan + UBSan | PASS |
| Existing UEFI host tests, GCC ASan + UBSan, 14 сценариев | PASS |
| `/dev/null` как source/destination disk packager | PASS: отклонён по stat до открытия устройства |
| Source/output hardlink alias | PASS: отклонён, source сохранён |
| Искусственный отказ os.replace | PASS: прежний output сохранён, temporary удалён |
| Output symlink к другому файлу | PASS: ссылка заменена, целевой файл не затронут |
| Fake-QEMU с BOOT_ERROR + положительными markers | Выявлен описанный P2 false positive |

Машиночитаемое подтверждение дополнительных проверок: `review-host/results.json`.

## Реальные журналы интеграции root

Повторно прочитан финальный `build/test-results/results.json` после исправления runner: 13 пунктов PASS — host boot_info/UEFI/Python (disk 6 + serial regression 4); две repro builds; сравнение всех четырёх artifacts; normal/negative target builds; x86 normal/invalid; ARM EL1 normal/invalid и EL2 normal. В x86 serial реально видны вход UEFI, EBS OK, kernel_main, validated v1 и правильные PASS/halt markers; negative прекращается на magic error. ARM журналы подтверждают initial EL1/EL2, current EL1, DTB header OK и общий handoff либо ожидаемый magic error. Эти журналы не содержат обнаруженного BOOT_ERROR+PASS сочетания; находка относится к строгости runner'а.

Ранний x86 сбой отсутствующего VGA ROM был окружением до исполнения guest; текущий runner использует `-vga none`, последующие журналы реально дошли до ядра.

## Privacy scan и пределы

На момент проверки 31 исходный файл вне `.git/`, `build/` и Python cache: известных признаков GitHub/OpenAI/AWS/Slack tokens, JWT, private keys, credential URL, email и строковых secret/password/API key assignments не найдено. EFI/ELF/IMG/firmware/object binaries в исходном наборе отсутствуют. Это проверка известных признаков, а не гарантия отсутствия неизвестного скрытого секрета. После добавления финальных docs/logs root должен проверить итоговый staged-набор.

Нет подтверждения VirtualBox, Android/Termux host, физического PC или ARM устройства. Нет allocator, scheduler, filesystem, production ABI, secure boot или безопасности промышленной ОС. Указанные пределы соответствуют текущим ADR и объёму T-P1.0.

## Итоговый статус review

**PASS в пределах T-P1.0. Открытых блокирующих или P2-находок нет.** Выявленная ошибка строгости serial smoke исправлена и повторно проверена. Архитектурные и платформенные пределы, перечисленные выше, сохраняются. Финальный --all root после исправления прочитан: все 13 пунктов PASS, включая обновлённый host-python. Итоговый staged/privacy набор после добавления документов относится к финальной интеграции родительского агента.
