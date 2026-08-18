@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "REPO_ROOT=%%~fI"

set "ENGINE_DIR=%REPO_ROOT%\KraftonEngine"
set "THIRDPARTY_DIR=%ENGINE_DIR%\ThirdParty"
set "PHYSX_BUILD_ROOT=%ENGINE_DIR%\Build"
set "PHYSX_LEGACY_ROOT=%REPO_ROOT%\PhysX"
set "PHYSX_OLD_SOURCE_ROOT=%THIRDPARTY_DIR%\PhysXSource"
set "PHYSX_ROOT=%PHYSX_BUILD_ROOT%\PhysX"
set "PHYSX_SDK_DIR=%PHYSX_ROOT%\physx"
set "PHYSX_STAGE_ROOT=%ENGINE_DIR%\ThirdParty\PhysX"
set "PHYSX_PRESET=vc17win64"
set "PHYSX_BUILD_DIR=%PHYSX_SDK_DIR%\compiler\%PHYSX_PRESET%"

set "PHYSX_GIT_URL=https://github.com/NVIDIAGameWorks/PhysX.git"
rem The public NVIDIA GameWorks repository exposes the PhysX 4.1 line as the
rem 4.1 branch. The old NuGet build number 4.1.229882250 does not map to a
rem public tag in that repository, so the branch is the stable source ref here.
set "PHYSX_GIT_REF=4.1"

echo [PhysX] Preparing PhysX dependency...

if not exist "%THIRDPARTY_DIR%\" mkdir "%THIRDPARTY_DIR%"
if not exist "%PHYSX_BUILD_ROOT%\" mkdir "%PHYSX_BUILD_ROOT%"

if not exist "%PHYSX_ROOT%\" (
    if exist "%PHYSX_LEGACY_ROOT%\physx\" (
        echo [PhysX] Moving existing PhysX source from "%PHYSX_LEGACY_ROOT%" to "%PHYSX_ROOT%"...
        move "%PHYSX_LEGACY_ROOT%" "%PHYSX_ROOT%" >nul
        if errorlevel 1 (
            call :Error "Failed to move PhysX source to %PHYSX_ROOT%."
            exit /b 1
        )
    ) else if exist "%PHYSX_OLD_SOURCE_ROOT%\physx\" (
        echo [PhysX] Moving existing PhysX source from "%PHYSX_OLD_SOURCE_ROOT%" to "%PHYSX_ROOT%"...
        move "%PHYSX_OLD_SOURCE_ROOT%" "%PHYSX_ROOT%" >nul
        if errorlevel 1 (
            call :Error "Failed to move PhysX source to %PHYSX_ROOT%."
            exit /b 1
        )
    )
)

if not exist "%PHYSX_ROOT%\" (
    where /q git
    if errorlevel 1 (
        call :Error "git.exe was not found. Install Git or add it to PATH."
        exit /b 1
    )

    echo [PhysX] Cloning %PHYSX_GIT_URL% ref %PHYSX_GIT_REF% into "%PHYSX_ROOT%"...
    git clone --recursive --branch "%PHYSX_GIT_REF%" "%PHYSX_GIT_URL%" "%PHYSX_ROOT%"
    if errorlevel 1 (
        call :Error "Failed to clone PhysX."
        exit /b 1
    )
) else (
    echo [PhysX] Using existing PhysX source at "%PHYSX_ROOT%".
)

if not exist "%PHYSX_SDK_DIR%\" (
    call :Error "PhysX source layout is invalid. Expected %PHYSX_SDK_DIR%."
    exit /b 1
)

set "PM_python_PATH=%REPO_ROOT%\Scripts\python"

call :CopyFile "%REPO_ROOT%\Scripts\PhysX\generate_projects.bat" "%PHYSX_SDK_DIR%\generate_projects.bat"
if errorlevel 1 exit /b 1

call :CopyFile "%REPO_ROOT%\Scripts\PhysX\cmake_generate_projects.py" "%PHYSX_SDK_DIR%\buildtools\cmake_generate_projects.py"
if errorlevel 1 exit /b 1

call :CopyFile "%REPO_ROOT%\Scripts\vc17win64.xml" "%PHYSX_SDK_DIR%\buildtools\presets\public\vc17win64.xml"
if errorlevel 1 exit /b 1

call :HasStagedArtifacts
if not errorlevel 1 (
    call :AskRebuild
    if errorlevel 2 (
        echo [PhysX] Skipping PhysX rebuild; staged artifacts are already available.
        exit /b 0
    )
)

call :ResolveCMake
if errorlevel 1 exit /b 1

call :GenerateProjects
if errorlevel 1 exit /b 1

call :BuildConfig Debug
if errorlevel 1 exit /b 1

call :BuildConfig Release
if errorlevel 1 exit /b 1

call :StageIncludes
if errorlevel 1 exit /b 1

call :StageConfig Debug
if errorlevel 1 exit /b 1

call :StageConfig Release
if errorlevel 1 exit /b 1

echo [PhysX] PhysX is ready at "%PHYSX_STAGE_ROOT%".
exit /b 0

:HasStagedArtifacts
call :RequireFile "%PHYSX_STAGE_ROOT%\Include\physx\PxPhysicsAPI.h"
if errorlevel 1 exit /b 1

call :RequireFile "%PHYSX_STAGE_ROOT%\Include\pxshared\foundation\Px.h"
if errorlevel 1 exit /b 1

for %%C in (Debug Release) do (
    for %%L in (PhysX_64.lib PhysXCommon_64.lib PhysXFoundation_64.lib PhysXExtensions_static_64.lib PhysXPvdSDK_static_64.lib) do (
        call :RequireFile "%PHYSX_STAGE_ROOT%\Lib\x64\%%C\%%L"
        if errorlevel 1 exit /b 1
    )
    for %%D in (PhysX_64.dll PhysXCommon_64.dll PhysXFoundation_64.dll) do (
        call :RequireFile "%PHYSX_STAGE_ROOT%\Bin\x64\%%C\%%D"
        if errorlevel 1 exit /b 1
    )
)

exit /b 0

:RequireFile
if exist "%~1" exit /b 0
exit /b 1

:AskRebuild
if /I "%PHYSX_REBUILD%"=="Y" exit /b 1
if /I "%PHYSX_REBUILD%"=="YES" exit /b 1
if /I "%PHYSX_REBUILD%"=="1" exit /b 1
if /I "%PHYSX_REBUILD%"=="N" exit /b 2
if /I "%PHYSX_REBUILD%"=="NO" exit /b 2
if /I "%PHYSX_REBUILD%"=="0" exit /b 2

echo [PhysX] Existing staged PhysX artifacts were found.
set "REBUILD_ANSWER="
set /p "REBUILD_ANSWER=[PhysX] Rebuild PhysX anyway? [Y/N]: "
if /I "%REBUILD_ANSWER%"=="Y" exit /b 1
if /I "%REBUILD_ANSWER%"=="YES" exit /b 1
exit /b 2

:CopyFile
set "COPY_SRC=%~1"
set "COPY_DST=%~2"

if not exist "%COPY_SRC%" (
    call :Error "Required source file is missing: %COPY_SRC%."
    exit /b 1
)

for %%D in ("%COPY_DST%") do (
    if not exist "%%~dpD" mkdir "%%~dpD"
)

copy /Y "%COPY_SRC%" "%COPY_DST%" >nul
if errorlevel 1 (
    call :Error "Failed to copy %COPY_SRC% to %COPY_DST%."
    exit /b 1
)

echo [PhysX] Copied "%COPY_SRC%" to "%COPY_DST%".
exit /b 0

:GenerateProjects
echo [PhysX] Generating PhysX projects with preset %PHYSX_PRESET%...
pushd "%PHYSX_SDK_DIR%"
if errorlevel 1 (
    call :Error "Failed to enter %PHYSX_SDK_DIR%."
    exit /b 1
)

call generate_projects.bat "%PHYSX_PRESET%"
set "GENERATE_RESULT=%ERRORLEVEL%"
popd

if not "%GENERATE_RESULT%"=="0" (
    call :Error "PhysX project generation failed."
    exit /b 1
)

if not exist "%PHYSX_BUILD_DIR%\" (
    call :Error "PhysX project generation did not create %PHYSX_BUILD_DIR%."
    exit /b 1
)

exit /b 0

:ResolveCMake
if exist "%PHYSX_ROOT%\externals\cmake\x64\bin\cmake.exe" (
    set "CMAKE_EXE=%PHYSX_ROOT%\externals\cmake\x64\bin\cmake.exe"
    set "PM_CMAKE_PATH=%PHYSX_ROOT%\externals\cmake\x64"
    exit /b 0
)

if exist "%ENGINE_DIR%\Build\Tools\CMake\bin\cmake.exe" (
    set "CMAKE_EXE=%ENGINE_DIR%\Build\Tools\CMake\bin\cmake.exe"
    set "PM_CMAKE_PATH=%ENGINE_DIR%\Build\Tools\CMake"
    exit /b 0
)

where /q cmake
if errorlevel 1 (
    call :Error "cmake.exe was not found. Install CMake 3.12 or newer or add it to PATH."
    exit /b 1
)

set "CMAKE_EXE=cmake"
exit /b 0

:BuildConfig
set "BUILD_CONFIG=%~1"

echo [PhysX] Building PhysX %BUILD_CONFIG%...
"%CMAKE_EXE%" --build "%PHYSX_BUILD_DIR%" --config "%BUILD_CONFIG%" -- /m
if errorlevel 1 (
    call :Error "PhysX %BUILD_CONFIG% build failed."
    exit /b 1
)

exit /b 0

:StageIncludes
echo [PhysX] Staging PhysX headers...

if not exist "%PHYSX_SDK_DIR%\include\" (
    call :Error "Missing PhysX include directory: %PHYSX_SDK_DIR%\include."
    exit /b 1
)

if not exist "%PHYSX_ROOT%\pxshared\include\" (
    call :Error "Missing PxShared include directory: %PHYSX_ROOT%\pxshared\include."
    exit /b 1
)

call :RobocopyDir "%PHYSX_SDK_DIR%\include" "%PHYSX_STAGE_ROOT%\Include\physx"
if errorlevel 1 exit /b 1

call :RobocopyDir "%PHYSX_ROOT%\pxshared\include" "%PHYSX_STAGE_ROOT%\Include\pxshared"
if errorlevel 1 exit /b 1

exit /b 0

:RobocopyDir
set "ROBO_SRC=%~1"
set "ROBO_DST=%~2"

robocopy "%ROBO_SRC%" "%ROBO_DST%" *.* /MIR /NFL /NDL /NJH /NJS /NP >nul
set "ROBO_RESULT=%ERRORLEVEL%"
if !ROBO_RESULT! GEQ 8 (
    call :Error "robocopy failed from %ROBO_SRC% to %ROBO_DST% with exit code !ROBO_RESULT!."
    exit /b 1
)

exit /b 0

:FindPhysXArtifactDir
set "ARTIFACT_CONFIG=%~1"
set "PHYSX_ARTIFACT_DIR="
set "PHYSX_BIN_SEARCH_ROOT=%PHYSX_SDK_DIR%\bin"

if not exist "%PHYSX_BIN_SEARCH_ROOT%\" (
    call :Error "PhysX bin directory was not found: %PHYSX_BIN_SEARCH_ROOT%."
    exit /b 1
)

for /f "delims=" %%D in ('dir /b /ad /o-d "%PHYSX_BIN_SEARCH_ROOT%\win.x86_64.*.md" 2^>nul') do (
    if not defined PHYSX_ARTIFACT_DIR (
        if exist "%PHYSX_BIN_SEARCH_ROOT%\%%D\%ARTIFACT_CONFIG%\" (
            set "PHYSX_ARTIFACT_DIR=%PHYSX_BIN_SEARCH_ROOT%\%%D\%ARTIFACT_CONFIG%"
        )
    )
)

if not defined PHYSX_ARTIFACT_DIR (
    call :Error "No PhysX %ARTIFACT_CONFIG% artifact directory was found under %PHYSX_BIN_SEARCH_ROOT%\win.x86_64.*.md."
    exit /b 1
)

echo [PhysX] Using PhysX artifacts from "%PHYSX_ARTIFACT_DIR%".
exit /b 0

:StageConfig
set "STAGE_CONFIG=%~1"
set "LIB_DEST=%PHYSX_STAGE_ROOT%\Lib\x64\%STAGE_CONFIG%"
set "BIN_DEST=%PHYSX_STAGE_ROOT%\Bin\x64\%STAGE_CONFIG%"
set "SOURCE_CONFIG=%STAGE_CONFIG%"
if /I "%STAGE_CONFIG%"=="Debug" set "SOURCE_CONFIG=debug"
if /I "%STAGE_CONFIG%"=="Release" set "SOURCE_CONFIG=release"

echo [PhysX] Staging PhysX %STAGE_CONFIG% libs and DLLs...

if not exist "%LIB_DEST%\" mkdir "%LIB_DEST%"
if not exist "%BIN_DEST%\" mkdir "%BIN_DEST%"

call :FindPhysXArtifactDir "%SOURCE_CONFIG%"
if errorlevel 1 exit /b 1

set "FOUND_LIB=0"
for %%F in ("%PHYSX_ARTIFACT_DIR%\PhysX*.lib") do (
    if exist "%%~fF" (
        copy /Y "%%~fF" "%LIB_DEST%\" >nul
        if errorlevel 1 (
            call :Error "Failed to stage library %%~fF."
            exit /b 1
        )
        set "FOUND_LIB=1"
    )
)

set "FOUND_DLL=0"
for %%F in ("%PHYSX_ARTIFACT_DIR%\PhysX*.dll") do (
    if exist "%%~fF" (
        copy /Y "%%~fF" "%BIN_DEST%\" >nul
        if errorlevel 1 (
            call :Error "Failed to stage DLL %%~fF."
            exit /b 1
        )
        set "FOUND_DLL=1"
    )
)

if "%FOUND_LIB%"=="0" (
    call :Error "No PhysX %STAGE_CONFIG% .lib files were found under %PHYSX_ARTIFACT_DIR%."
    exit /b 1
)

if "%FOUND_DLL%"=="0" (
    call :Error "No PhysX %STAGE_CONFIG% .dll files were found under %PHYSX_ARTIFACT_DIR%."
    exit /b 1
)

exit /b 0

:Error
echo [PhysX] ERROR: %~1
exit /b 1
