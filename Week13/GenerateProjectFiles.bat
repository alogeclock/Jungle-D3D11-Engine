@echo off
setlocal EnableExtensions

set "ROOT_DIR=%~dp0"
set "NO_PAUSE="

if /I "%~1"=="--no-pause" (
    set "NO_PAUSE=1"
    shift /1
)

call :BootstrapBuildTools
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" goto DONE

if exist "%ROOT_DIR%KraftonEngine\Build\Tools\CMake\bin\cmake.exe" (
    set "PATH=%ROOT_DIR%KraftonEngine\Build\Tools\CMake\bin;%PATH%"
)

call "%ROOT_DIR%Scripts\SetupPhysX.bat"
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" goto DONE

call "%ROOT_DIR%Scripts\SetupNvCloth.bat"
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

:BootstrapBuildTools
where /q powershell.exe
if errorlevel 1 (
    echo PowerShell is required to bootstrap Python and CMake.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%ROOT_DIR%Scripts\BootstrapBuildTools.ps1" -RepoRoot "%ROOT_DIR%."
exit /b %ERRORLEVEL%
