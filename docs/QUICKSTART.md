# StyleOS — быстрый старт проверок

[Точка входа](PROJECT.md) · [Полные инструкции и регрессии](TESTING.md) · [Отчёт по графике](test-results/desktop-display/README.md)

Это команды для инженерных образов. Устанавливать их на физический диск ПК не нужно. В момент среза [публичного выпуска нет](https://github.com/LPYURA2/OS/releases).

## Сборка двух диагностических x86‑64 образов

В Linux нужны Python 3, Clang и LLD. Для локального графического QEMU-теста также нужны QEMU x86‑64, OVMF и стандартный VGA ROM.

```sh
git clone https://github.com/LPYURA2/OS.git styleos
cd styleos
git checkout --detach c162995c4390686bda54468f68f40164b3c4f3bc
python3 tools/build.py --arch x86_64 --output build/virtualbox-base
python3 tools/build.py --arch x86_64 --test-desktop-display --output build/virtualbox-display
```

Проверить картинку и отказ без VGA в QEMU можно одной командой. Выходной каталог должен быть новым.

```sh
python3 tools/test_desktop_display.py --output build/desktop-proof
```

Если QEMU сообщает, что не нашёл `vgabios-stdvga.bin`, проверьте каталог ROM эмулятора. Раннер принимает переменную `QEMU_DATA_DIR`. Ошибка поиска ROM до загрузки гостя не является отказом ядра.

## VirtualBox

На ПК x86‑64 преобразуйте оба сырых образа в VDI. Конвертируются только файлы из каталогов сборки.

```sh
VBoxManage convertfromraw build/virtualbox-base/x86_64/styleos.img build/styleos-base-x64.vdi --format VDI
VBoxManage convertfromraw build/virtualbox-display/x86_64/styleos.img build/styleos-display-check-x64.vdi --format VDI
```

1. Создайте одну тестовую VM x86‑64 с EFI и SATA. Для первой попытки — 1 vCPU, 256 МБ, VMSVGA, без 3D. ISO не нужен. Диск данных к этим диагностическим образам не подключается.
2. Включите гостевой COM1 с записью в файл. Подключите базовый VDI. После старта ожидаются `HANDOFF PASS x86_64`, `MEMMAP PASS` и `KERNEL HALT`. Пустой экран допустим.
3. Выключите VM, отсоедините базовый VDI **без удаления файла** и подключите графический в тот же порт. Используйте отдельный COM1-лог. Проверяются `DISPLAY PASS` и видимый экран `STYLEOS / DISPLAY CHECK`. После штатного halt экран не реагирует на ввод.
4. Сохраните версию VirtualBox, SHA образа, отдельные журналы и снимок графического прогона. Пока такого результата нет, VirtualBox не считается пройденным.

## Android/Termux

Принятый AArch64 диск и загрузчик приложения — отдельный тест на `40c3987` с `CC=clang python tools/test_disk_app.py --output build/termux-tp18b`. Для двух x86‑64 VDI на ARM-телефоне использовался `qemu-system-x86_64` с x86‑64 UEFI, а не ARM-эмулятор. Подтверждение по COM1 есть. Это не проверка VirtualBox и не нативный порт. [Фактические результаты](test-results/owner-2026-11-17.md)
