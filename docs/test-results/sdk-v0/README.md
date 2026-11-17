# StyleOS SDK 0.1 Preview — проверка сборки и пакета

Ветка `feature/styleos-sdk-v0` основана на
`932713149c5b4d4541c6ec60120c3141b7a78e1c`. Новые файлы SDK и
документация не входят в сборку ядра.

| Проверка | Результат |
|---|---|
| SDK на Linux | 10/10 PASS. Внешний проект с пробелами в пути, реальная сборка и воспроизводимый пакет, правка шаблона, пошаговый режим, отказ от повреждённых и несовместимых пакетов. |
| Логика «Презентаций» | PASS на хосте. Точные байты текста, ограничение длины и отказ при недостаточном буфере. |
| Прежние регрессии StyleOS Linux/QEMU | 57/57 PASS, включая 299 Python-тестов и повторяемость обычной сборки. |
| Windows-комплект | Собран NSIS x64, архив NSIS прошёл контроль целостности 7-Zip (352 файла), извлечённый `presentations.tea` побайтно совпал и прошёл verify. Запуск установщика в Windows не испытан в этой среде. |
| Установка / запуск .tea внутри StyleOS | Не реализованы. VirtualBox и Termux не принимаются по этим проверкам. |

Воспроизведение на Linux

```sh
python3 -m unittest discover -s sdk/tests -v
python3 sdk/styleos_sdk.py build sdk/examples/presentations --output build/sdk/presentations.tea
python3 sdk/styleos_sdk.py verify build/sdk/presentations.tea
python3 tools/test.py --all --output build/sdk-base-regression
```

Тестировались Python 3.12.14, Clang/LLD 19.1.7, GCC 14.2.0 и QEMU 10.0.13.
SHA-256 собранного примера с этим Linux toolchain
`4af5368b501573921cb21add784d65968732203d2a9397d5106a39c8ef42975e`.
Другая версия Clang/LLD может дать другой корректный пакет.

SHA-256 собранного `StyleOS-SDK-Setup-x64.exe`
`2fcdbd8c9d1ce59d2cf57b15eba3d6f8587c6e2078fea3c9b122f10b5e874c8f`.

См. [устройство SDK](../../../sdk/README.md) и
[рецепт Windows-комплекта](../../../sdk/windows/README.md).
