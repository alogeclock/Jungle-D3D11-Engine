@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SOLUTION_DIR=%~dp0"
set "CONFIGURATION=Shipping"
set "PLATFORM=x64"
set "STAGE_DIR=%SOLUTION_DIR%ShippingBuild"
set "COOKED_ROOT=%SOLUTION_DIR%Saved\Cooked\%CONFIGURATION%"
set "COOKED_SCENE_DIR=%COOKED_ROOT%\Asset\Content\Scene"
set "MANIFEST_FILE=%STAGE_DIR%\BuildManifest.txt"
set "NO_PAUSE=0"
if /I "%~1"=="-NoPause" set "NO_PAUSE=1"

call :FindProject || goto :Failed

set "BUILD_OUTPUT=%PROJECT_DIR%\Bin\%CONFIGURATION%"
set "EXE_NAME=%PROJECT_NAME%.exe"

echo ============================================
echo  %PROJECT_NAME% BuildCookRun
echo  Configuration: %CONFIGURATION% %PLATFORM%
echo ============================================

call :FindVisualStudio || goto :Failed

echo.
echo [1/6] Build
msbuild "%SOLUTION_FILE%" /p:Configuration=%CONFIGURATION% /p:Platform=%PLATFORM% /m /v:minimal
if errorlevel 1 (
    echo ERROR: MSBuild failed.
    goto :Failed
)

if not exist "%BUILD_OUTPUT%\%EXE_NAME%" (
    echo ERROR: Build output was not found: "%BUILD_OUTPUT%\%EXE_NAME%"
    goto :Failed
)

echo.
echo [2/6] Cook maps
call :CleanDir "%COOKED_ROOT%" || goto :Failed
mkdir "%COOKED_SCENE_DIR%" || goto :Failed

pushd "%PROJECT_DIR%" >nul
"%BUILD_OUTPUT%\%EXE_NAME%" -cook -cookoutput="%COOKED_SCENE_DIR%"
set "COOK_RESULT=%ERRORLEVEL%"
popd >nul
if not "%COOK_RESULT%"=="0" (
    echo ERROR: Cook failed.
    goto :Failed
)

echo.
echo [3/6] Build cooked runtime root
if not exist "%PROJECT_DIR%\Asset" (
    echo ERROR: Required directory was not found: "%PROJECT_DIR%\Asset"
    goto :Failed
)

rem Asset is assembled into Saved/Cooked first. Editor scene sources stay out of ShippingBuild.
robocopy "%PROJECT_DIR%\Asset" "%COOKED_ROOT%\Asset" /MIR /NFL /NDL /NJH /NJS /NP /XD "%PROJECT_DIR%\Asset\Content\Scene" /XF *.Scene *.scene *.pdb *.ilk *.obj.recipe
if errorlevel 8 (
    echo ERROR: Failed to build cooked Asset root.
    goto :Failed
)

echo.
echo [4/6] Prepare staging directory
call :CleanDir "%STAGE_DIR%" || goto :Failed
mkdir "%STAGE_DIR%" || goto :Failed

echo.
echo [5/6] Stage runtime files
copy /y "%BUILD_OUTPUT%\%EXE_NAME%" "%STAGE_DIR%\" >nul
if errorlevel 1 (
    echo ERROR: Failed to copy executable.
    goto :Failed
)

if exist "%BUILD_OUTPUT%\*.dll" copy /y "%BUILD_OUTPUT%\*.dll" "%STAGE_DIR%\" >nul
if exist "%BUILD_OUTPUT%\*.ini" copy /y "%BUILD_OUTPUT%\*.ini" "%STAGE_DIR%\" >nul
if exist "%PROJECT_DIR%\*.manifest" copy /y "%PROJECT_DIR%\*.manifest" "%STAGE_DIR%\" >nul

rem Asset is always staged from Saved/Cooked, never directly from the project Asset root.
call :MirrorDir "%COOKED_ROOT%\Asset" "%STAGE_DIR%\Asset" || goto :Failed

rem Unreal-style loose runtime staging: copy project runtime roots automatically,
rem while excluding source, build products, caches, dependencies, and editor saves.
call :StageProjectRuntimeDirs || goto :Failed

echo.
echo [6/6] Verify staged build
call :WriteManifest || goto :Failed
call :VerifyStagedBuild || goto :Failed

echo.
echo ============================================
echo  Build complete: %STAGE_DIR%
echo  Cooked maps staged: !UMAPPED!
echo  Manifest: %MANIFEST_FILE%
echo ============================================
echo.
call :PauseIfNeeded
exit /b 0

:FindProject
pushd "%SOLUTION_DIR%" >nul
for %%F in (*.sln) do if not defined SOLUTION_FILE set "SOLUTION_FILE=%SOLUTION_DIR%%%F"
popd >nul
if not defined SOLUTION_FILE (
    echo ERROR: Solution file was not found in "%SOLUTION_DIR%".
    exit /b 1
)

for /d %%D in ("%SOLUTION_DIR%*") do (
    for %%P in ("%%~fD\*.vcxproj") do (
        if exist "%%~fP" if not defined PROJECT_FILE (
            set "PROJECT_FILE=%%~fP"
            set "PROJECT_DIR=%%~dpP"
            set "PROJECT_NAME=%%~nP"
        )
    )
)

if not defined PROJECT_FILE (
    echo ERROR: Project file was not found under "%SOLUTION_DIR%".
    exit /b 1
)
if "%PROJECT_DIR:~-1%"=="\" set "PROJECT_DIR=%PROJECT_DIR:~0,-1%"
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

:StageProjectRuntimeDirs
for /d %%D in ("%PROJECT_DIR%\*") do (
    set "DIR_NAME=%%~nxD"
    call :IsExcludedRuntimeDir "!DIR_NAME!"
    if errorlevel 1 (
        call :MirrorDir "%%~fD" "%STAGE_DIR%\!DIR_NAME!" || exit /b 1
    )
)
exit /b 0

:IsExcludedRuntimeDir
set "NAME=%~1"
if /I "%NAME%"=="Asset" exit /b 0
if /I "%NAME%"=="Bin" exit /b 0
if /I "%NAME%"=="Build" exit /b 0
if /I "%NAME%"=="Intermediate" exit /b 0
if /I "%NAME%"=="Saves" exit /b 0
if /I "%NAME%"=="Source" exit /b 0
if /I "%NAME%"=="ThirdParty" exit /b 0
if /I "%NAME%"=="packages" exit /b 0
if /I "%NAME%"==".vs" exit /b 0
exit /b 1

:CleanDir
if exist "%~1" (
    rmdir /s /q "%~1"
    if exist "%~1" (
        echo ERROR: Could not clean "%~1".
        exit /b 1
    )
)
exit /b 0

:MirrorDir
set "SRC=%~1"
set "DST=%~2"
if not exist "%SRC%" (
    echo ERROR: Required directory was not found: "%SRC%"
    exit /b 1
)
robocopy "%SRC%" "%DST%" /MIR /NFL /NDL /NJH /NJS /NP /XF *.pdb *.ilk *.obj.recipe
if errorlevel 8 (
    echo ERROR: Failed to stage "%SRC%" to "%DST%".
    exit /b 1
)
exit /b 0

:WriteManifest
if exist "%MANIFEST_FILE%" del /q "%MANIFEST_FILE%" >nul 2>&1
pushd "%STAGE_DIR%" >nul
(
    echo # %PROJECT_NAME% %CONFIGURATION% %PLATFORM% staged files
    echo # Root: %STAGE_DIR%
    for /f "delims=" %%F in ('dir /a:-d /b /s') do (
        set "ABS=%%F"
        echo !ABS:%STAGE_DIR%\=!
    )
) > "%MANIFEST_FILE%"
popd >nul
exit /b 0

:VerifyStagedBuild
call :RequireFile "%STAGE_DIR%\%EXE_NAME%" || exit /b 1
call :RequireDir "%STAGE_DIR%\Asset\Content" || exit /b 1
call :RequireDir "%STAGE_DIR%\Settings" || exit /b 1
call :RequireDir "%STAGE_DIR%\Scripts" || exit /b 1
call :RequireDir "%STAGE_DIR%\Shaders" || exit /b 1
call :RequireFile "%STAGE_DIR%\Settings\Resource\ProjectResourcePaths.ini" || exit /b 1
call :RequireFile "%STAGE_DIR%\Settings\Resource\DefaultContentResources.ini" || exit /b 1

set /a UMAPPED=0
for /r "%STAGE_DIR%\Asset\Content\Scene" %%F in (*.umap) do set /a UMAPPED+=1
if !UMAPPED! EQU 0 (
    echo ERROR: No cooked .umap files were staged.
    exit /b 1
)

set /a SCENE_JSON=0
for /r "%STAGE_DIR%\Asset\Content\Scene" %%F in (*.Scene *.scene) do set /a SCENE_JSON+=1
if !SCENE_JSON! GTR 0 (
    echo ERROR: Editor scene files were staged into ShippingBuild.
    exit /b 1
)

call :RequireFile "%STAGE_DIR%\Asset\Content\Data\Dialogues\play.dialogue.json" || exit /b 1
call :RequireFile "%STAGE_DIR%\Asset\Content\Data\Dialogues\game_result.dialogue.json" || exit /b 1
call :RequireFile "%STAGE_DIR%\Asset\Content\Data\Scenarios\story.json" || exit /b 1
call :RequireMeshAsset "%STAGE_DIR%\Asset\Content\Model\BasicShape\cube.obj" "%STAGE_DIR%\Asset\Content\Model\BasicShape\Cache\cube.bin" || exit /b 1
call :RequireMeshAsset "%STAGE_DIR%\Asset\Content\Model\BasicShape\sphere.obj" "%STAGE_DIR%\Asset\Content\Model\BasicShape\Cache\sphere.bin" || exit /b 1
call :RequireMeshAsset "%STAGE_DIR%\Asset\Content\Model\BasicShape\cylinder.obj" "%STAGE_DIR%\Asset\Content\Model\BasicShape\Cache\cylinder.bin" || exit /b 1
call :RequireMeshAsset "%STAGE_DIR%\Asset\Content\Model\POD\Spaceship.obj" "%STAGE_DIR%\Asset\Content\Model\POD\Cache\Spaceship.bin" || exit /b 1
call :RequireMeshAsset "%STAGE_DIR%\Asset\Content\Model\SlideOrJump\SlideOrJump.obj" "%STAGE_DIR%\Asset\Content\Model\SlideOrJump\Cache\SlideOrJump.bin" || exit /b 1
exit /b 0

:RequireFile
if not exist "%~1" (
    echo ERROR: Required file is missing: "%~1"
    exit /b 1
)
exit /b 0

:RequireMeshAsset
if exist "%~1" exit /b 0
if exist "%~2" exit /b 0
echo ERROR: Required mesh is missing. Expected source or cache:
echo   Source: "%~1"
echo   Cache:  "%~2"
exit /b 1

:RequireDir
if not exist "%~1\" (
    echo ERROR: Required directory is missing: "%~1"
    exit /b 1
)
exit /b 0

:PauseIfNeeded
if "%NO_PAUSE%"=="0" pause
exit /b 0

:Failed
echo.
echo ============================================
echo  Shipping build failed.
echo ============================================
echo.
call :PauseIfNeeded
exit /b 1
