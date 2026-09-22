@echo off
setlocal

call "%~dp0Scripts\RunPython.bat" "%~dp0Scripts\GenerateHeaders.py" --root "%~dp0KraftonEngine"
if errorlevel 1 goto Failed

call "%~dp0Scripts\RunPython.bat" "%~dp0Scripts\GenerateLuaBindings.py" --root "%~dp0KraftonEngine"
if errorlevel 1 goto Failed

call "%~dp0Scripts\RunPython.bat" "%~dp0Scripts\GenerateProjectFiles.py" %*
if errorlevel 1 goto Failed

set "RESULT=0"
goto Finish

:Failed
set "RESULT=%ERRORLEVEL%"

:Finish
if not defined NO_PAUSE pause
exit /b %RESULT%
