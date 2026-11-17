@echo off
setlocal
set "PROJ=%USERPROFILE%\StyleOSProjects\presentations"
if not exist "%PROJ%" (
  mkdir "%PROJ%" || goto failure
  copy "%~dp0sdk\examples\presentations\app.json" "%PROJ%\" >nul || goto failure
  copy "%~dp0sdk\examples\presentations\main.c" "%PROJ%\" >nul || goto failure
)
call "%~dp0StyleOS-SDK.cmd" build "%PROJ%" --output "%PROJ%\presentations.tea" || goto failure
call "%~dp0StyleOS-SDK.cmd" verify "%PROJ%\presentations.tea" || goto failure
pause
exit /b 0
:failure
echo Build failed. Check the output above.
pause
exit /b 1
