# StyleOS — актуальная точка продолжения после 755

## Контрольная ОС
- **744 неизменна**: `/mnt/data/styleos-recovery-744/StyleOS-Install-x64.iso`. Контрольная сумма в `docs/744-binary-checksums.txt`.
- У владельца также сохранены 737, 740, 743, 744 как готовые образы. Они не содержат исходного рабочего стола и полноценного `Documents`.

## Рабочая линия
- База: `feature/styleos-documents-recovery-754`.
- Новый этап: `feature/styleos-documents-runtime-755`.
- Изолированная модель + DOCX/TXT + Review + независимый UI `app`.
- Добавлен отдельный app runtime с file grants и окном через callbacks, freestanding ELF x86-64, экспериментальный `Documents.tea` и самостоятельное тестирование реального ELF в Linux-хосте.
- **Это НЕ установленное приложение в гостевой ОС 744 и НЕ новая ISO.** 744 loader ABI неизвестен. Пакет неподписан доверенным ключом и явно помечен несовместимым с 744.

## Тесты
`sh tools/test_documents_755.sh`: host GCC/Clang + sanitizers + 1200 DOCX negative/fuzz + 40 edit rounds + 14 Python + SHA/ZIP/ELF verifier + reproducible TEA + actual ELF x86-64 host invocation PASS. **QEMU, VirtualBox и StyleOS guest — NOT RUN.**

## Следующая работа
- Воссоздать совместимый runtime системных приложений и безопасный установщик `.tea` по реальному источнику, не заменяя 744 древней десктопной веткой.
- Подключить независимый редактор к window/file broker и ISO с неизменными функциями других приложений.
- Реализовать все команды 14 макетов, особенно форматирование, объекты, полноценный DOCX, PDF и автоматизацию.
- Затем строгая сквозная приёмка в QEMU + последующая ручная VirtualBox.
- Каждый новый этап в отдельной ветке, исходники/тесты/сборка в архиве. Не выдавать хост-тесты за приёмку ОС.
