@echo off
setlocal
set "CLANG=%~dp0toolchain\bin\clang-21.exe"
set "LD_LLD=%~dp0toolchain\bin\ld.lld.exe"
if not exist "%CLANG%" (
  echo SDK FAIL: bundled Clang is missing.
  exit /b 1
)
if not exist "%LD_LLD%" (
  echo SDK FAIL: bundled LLD is missing.
  exit /b 1
)
"%~dp0runtime\python.exe" "%~dp0sdk\styleos_sdk.py" %*
exit /b %ERRORLEVEL%
