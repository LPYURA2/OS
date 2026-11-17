# Состав Windows-комплекта SDK Preview

В комплект входят части двух публичных дистрибутивов. Они не коммитятся в
исходный репозиторий. Штатные тексты лицензий копируются вместе с бинарниками.

- [LLVM-MinGW, выпуск 20251104 UCRT x86_64](https://github.com/mstorsjo/llvm-mingw/releases/tag/20251104) — Clang, LLD, их Windows DLL и заголовки Clang. Лицензии дистрибутива и уведомления — `toolchain/LICENSE.TXT`. Компилятор применяется к freestanding ELF и не требует MinGW sysroot при этой сборке.
- [CPython 3.14.10 Windows embeddable x64](https://www.python.org/downloads/release/python-31410/) — локальный интерпретатор SDK. Сопроводительный текст — `runtime/LICENSE.txt`.
- [NSIS](https://nsis.sourceforge.io/License) — сборщик установщика. Файл рецепта — `sdk/windows/setup.nsi` в репозитории.

SHA-256 исходных архивов закреплены в `sdk/windows/stage.py`. SDK не загружает эти архивы на ноутбуке пользователя.
