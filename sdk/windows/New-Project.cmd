@echo off
setlocal
call "%~dp0StyleOS-SDK.cmd" interactive new
set "RESULT=%ERRORLEVEL%"
pause
exit /b %RESULT%
