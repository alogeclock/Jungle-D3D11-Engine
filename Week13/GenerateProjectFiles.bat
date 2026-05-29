@echo off
setlocal EnableExtensions

set "ROOT_DIR=%~dp0"
set "NO_PAUSE="

if /I "%~1"=="--no-pause" (
    set "NO_PAUSE=1"
    shift /1
)

call "%ROOT_DIR%Scripts\SetupPhysX.bat"
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" goto DONE

"%ROOT_DIR%Scripts\python\python.exe" "%ROOT_DIR%Scripts\GenerateProjectFiles.py" %*
set "RESULT=%ERRORLEVEL%"

:DONE
echo.
if "%RESULT%"=="0" (
    echo GenerateProjectFiles completed successfully.
) else (
    echo GenerateProjectFiles failed with exit code %RESULT%.
)

if not defined NO_PAUSE pause

endlocal & exit /b %RESULT%
