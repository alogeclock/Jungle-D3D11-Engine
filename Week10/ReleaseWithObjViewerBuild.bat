@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SOLUTION_DIR=%~dp0"
set "CONFIGURATION=Release"
set "OBJVIEW_CONFIGURATION=ObjViewDebug"
set "PLATFORM=x64"
set "RELEASE_DIR=%SOLUTION_DIR%ReleaseBuild"
set "NO_PAUSE=0"
if /I "%~1"=="-NoPause" set "NO_PAUSE=1"

call :FindProject || goto :Failed

set "BUILD_OUTPUT=%PROJECT_DIR%\Bin\%CONFIGURATION%"
set "BUILD_OUTPUT_OBJVIEW=%PROJECT_DIR%\Bin\%OBJVIEW_CONFIGURATION%"
set "EXE_NAME=%PROJECT_NAME%.exe"

echo ============================================
echo  %PROJECT_NAME% Release + ObjViewer Build
echo ============================================

call :FindVisualStudio || goto :Failed

echo.
echo [1/4] Building %CONFIGURATION% %PLATFORM%
msbuild "%SOLUTION_FILE%" /p:Configuration=%CONFIGURATION% /p:Platform=%PLATFORM% /m /v:minimal
if errorlevel 1 (
    echo ERROR: MSBuild failed for %CONFIGURATION%.
    goto :Failed
)

echo.
echo [2/4] Building %OBJVIEW_CONFIGURATION% %PLATFORM%
msbuild "%SOLUTION_FILE%" /p:Configuration=%OBJVIEW_CONFIGURATION% /p:Platform=%PLATFORM% /m /v:minimal
if errorlevel 1 (
    echo ERROR: MSBuild failed for %OBJVIEW_CONFIGURATION%.
    goto :Failed
)

if not exist "%BUILD_OUTPUT%\%EXE_NAME%" (
    echo ERROR: Build output was not found: "%BUILD_OUTPUT%\%EXE_NAME%"
    goto :Failed
)
if not exist "%BUILD_OUTPUT_OBJVIEW%\%EXE_NAME%" (
    echo ERROR: ObjViewer build output was not found: "%BUILD_OUTPUT_OBJVIEW%\%EXE_NAME%"
    goto :Failed
)

echo.
echo [3/4] Preparing output directory
if exist "%RELEASE_DIR%" (
    rmdir /s /q "%RELEASE_DIR%"
    if exist "%RELEASE_DIR%" (
        echo ERROR: Could not clean "%RELEASE_DIR%".
        goto :Failed
    )
)
mkdir "%RELEASE_DIR%" || goto :Failed

echo.
echo [4/4] Copying files
copy /y "%BUILD_OUTPUT%\%EXE_NAME%" "%RELEASE_DIR%\" >nul || goto :Failed
copy /y "%BUILD_OUTPUT_OBJVIEW%\%EXE_NAME%" "%RELEASE_DIR%\ObjViewer.exe" >nul || goto :Failed
if exist "%BUILD_OUTPUT%\*.dll" copy /y "%BUILD_OUTPUT%\*.dll" "%RELEASE_DIR%\" >nul
if exist "%BUILD_OUTPUT%\*.ini" copy /y "%BUILD_OUTPUT%\*.ini" "%RELEASE_DIR%\" >nul

call :MirrorDir "%PROJECT_DIR%\Shaders" "%RELEASE_DIR%\Shaders" || goto :Failed
call :MirrorDir "%PROJECT_DIR%\Asset" "%RELEASE_DIR%\Asset" || goto :Failed
call :MirrorDir "%PROJECT_DIR%\Settings" "%RELEASE_DIR%\Settings" || goto :Failed
call :MirrorDir "%PROJECT_DIR%\Scripts" "%RELEASE_DIR%\Scripts" || goto :Failed

if exist "%PROJECT_DIR%\Data" (
    robocopy "%PROJECT_DIR%\Data" "%RELEASE_DIR%\Data" /MIR /NFL /NDL /NJH /NJS /NP /XF *.pdb *.ilk
    if errorlevel 8 goto :Failed
)

echo.
echo ============================================
echo  Build complete: %RELEASE_DIR%
echo ============================================
echo.
call :PauseIfNeeded
exit /b 0

:FindProject
pushd "%SOLUTION_DIR%" >nul
for %%F in (*.sln) do if not defined SOLUTION_FILE set "SOLUTION_FILE=%SOLUTION_DIR%%%F"
popd >nul
if not defined SOLUTION_FILE exit /b 1

for /d %%D in ("%SOLUTION_DIR%*") do (
    for %%P in ("%%~fD\*.vcxproj") do (
        if exist "%%~fP" if not defined PROJECT_FILE (
            set "PROJECT_FILE=%%~fP"
            set "PROJECT_DIR=%%~dpP"
            set "PROJECT_NAME=%%~nP"
        )
    )
)
if not defined PROJECT_FILE exit /b 1
exit /b 0

:FindVisualStudio
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: vswhere.exe was not found.
    exit /b 1
)
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VS_PATH=%%i"
if not defined VS_PATH (
    echo ERROR: Visual Studio was not found.
    exit /b 1
)
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -no_logo
exit /b %ERRORLEVEL%

:MirrorDir
if not exist "%~1" (
    echo ERROR: Required directory was not found: "%~1"
    exit /b 1
)
robocopy "%~1" "%~2" /MIR /NFL /NDL /NJH /NJS /NP
if errorlevel 8 exit /b 1
exit /b 0

:PauseIfNeeded
if "%NO_PAUSE%"=="0" pause
exit /b 0

:Failed
echo.
echo ============================================
echo  Release + ObjViewer build failed.
echo ============================================
echo.
call :PauseIfNeeded
exit /b 1
