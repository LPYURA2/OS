@echo off
setlocal
call "%~dp0StyleOS-SDK.cmd" interactive build
set "RESULT=%ERRORLEVEL%"
pause
exit /b %RESULT%
