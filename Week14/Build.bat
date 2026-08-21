@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "CONFIG=%~1"
if not defined CONFIG set "CONFIG=Debug"

if /i "%CONFIG%"=="Debug" goto ValidConfig
if /i "%CONFIG%"=="Release" goto ValidConfig
if /i "%CONFIG%"=="Game" goto ValidConfig
echo Unknown configuration: %CONFIG%
echo Usage: Build.bat [Debug^|Release^|Game]
set "RESULT=2"
goto Finish

:ValidConfig
echo ============================================
echo  Generate and build: %CONFIG% x64
echo ============================================

set "CALLER_NO_PAUSE=%NO_PAUSE%"
set "NO_PAUSE=1"
call "%~dp0GenerateProjectFiles.bat"
set "GENERATE_RESULT=!ERRORLEVEL!"
set "NO_PAUSE=%CALLER_NO_PAUSE%"
if not "!GENERATE_RESULT!"=="0" (
    set "RESULT=!GENERATE_RESULT!"
    goto Failed
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo Visual Studio Installer's vswhere.exe was not found.
    set "RESULT=1"
    goto Failed
)

set "MSBUILD="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do if not defined MSBUILD set "MSBUILD=%%i"
if not defined MSBUILD (
    echo MSBuild was not found. Install the Visual Studio C++ build tools.
    set "RESULT=1"
    goto Failed
)

"%MSBUILD%" "%~dp0KraftonEngine.sln" /t:Build /p:Configuration=%CONFIG% /p:Platform=x64 /m /nologo /v:minimal
if errorlevel 1 (
    set "RESULT=!ERRORLEVEL!"
    goto Failed
)

echo.
echo Build succeeded: KraftonEngine\Bin\%CONFIG%\KraftonEngine.exe
set "RESULT=0"
goto Finish

:Failed
echo.
echo Build failed with exit code %RESULT%.

:Finish
if not defined NO_PAUSE pause
exit /b %RESULT%
