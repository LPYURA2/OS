# Сборка установщика SDK Preview для Windows x64

Сами инструменты в исходниках SDK запакованы не будут. Получите два архива
по ссылкам в [THIRD-PARTY.md](THIRD-PARTY.md). Нужны Windows UCRT x86_64 из
LLVM-MinGW 20251104 и Windows embeddable amd64 из CPython 3.14.10. Их
проверяет `stage.py`. Для упаковки нужен Python 3.11+. Установщик строится
NSIS 3.11 с x64 Unicode stub.

```sh
python3 sdk/styleos_sdk.py build sdk/examples/presentations --output build/sdk/presentations.tea
python3 sdk/windows/stage.py --llvm-zip /path/llvm-mingw.zip --python-zip /path/python-embed.zip --package build/sdk/presentations.tea --output build/sdk/windows-payload
makensis "-DSDK_PAYLOAD=$PWD/build/sdk/windows-payload" "-DSDK_OUTPUT=$PWD/build/sdk/StyleOS-SDK-Setup-x64.exe" sdk/windows/setup.nsi
```

Установка пользовательская в %LOCALAPPDATA%. Исходники, которые пользователь
редактирует, и результаты его сборки лежат в %USERPROFILE%\StyleOSProjects,
а не в папке установленного SDK. Повторная сборка образа переносит только
проверенные компоненты из архивов, а не весь MinGW.
