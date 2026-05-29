@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "REPO_ROOT=%%~fI"

set "ENGINE_DIR=%REPO_ROOT%\KraftonEngine"
set "THIRDPARTY_DIR=%ENGINE_DIR%\ThirdParty"
set "NVCLOTH_BUILD_ROOT=%ENGINE_DIR%\Build"
set "NVCLOTH_ROOT=%NVCLOTH_BUILD_ROOT%\NvCloth"
set "NVCLOTH_SDK_DIR=%NVCLOTH_ROOT%\NvCloth"
set "NVCLOTH_PXSHARED_DIR=%NVCLOTH_ROOT%\PxShared"
set "NVCLOTH_STAGE_ROOT=%ENGINE_DIR%\ThirdParty\NvCloth"
set "NVCLOTH_BUILD_DIR=%NVCLOTH_SDK_DIR%\compiler\vc17win64-cmake"
set "NVCLOTH_OUTPUT_DIR=%NVCLOTH_SDK_DIR%\bin\win.x86_64.vc17.md"

set "NVCLOTH_GIT_URL=https://github.com/NVIDIAGameWorks/NvCloth.git"
set "NVCLOTH_GIT_REF=1.1.6"

echo [NvCloth] Preparing NvCloth dependency...

if not exist "%THIRDPARTY_DIR%\" mkdir "%THIRDPARTY_DIR%"
if not exist "%NVCLOTH_BUILD_ROOT%\" mkdir "%NVCLOTH_BUILD_ROOT%"

if not exist "%NVCLOTH_ROOT%\" (
    where /q git
    if errorlevel 1 (
        call :Error "git.exe was not found. Install Git or add it to PATH."
        exit /b 1
    )

    echo [NvCloth] Cloning %NVCLOTH_GIT_URL% ref %NVCLOTH_GIT_REF% into "%NVCLOTH_ROOT%"...
    git clone --recursive --branch "%NVCLOTH_GIT_REF%" "%NVCLOTH_GIT_URL%" "%NVCLOTH_ROOT%"
    if errorlevel 1 (
        call :Error "Failed to clone NvCloth."
        exit /b 1
    )
) else (
    echo [NvCloth] Using existing NvCloth source at "%NVCLOTH_ROOT%".
)

if not exist "%NVCLOTH_SDK_DIR%\compiler\cmake\windows\CMakeLists.txt" (
    call :Error "NvCloth source layout is invalid. Expected %NVCLOTH_SDK_DIR%\compiler\cmake\windows\CMakeLists.txt."
    exit /b 1
)

if exist "%NVCLOTH_ROOT%\.git\" (
    git -C "%NVCLOTH_ROOT%" submodule update --init --recursive
    if errorlevel 1 (
        call :Error "Failed to update NvCloth submodules."
        exit /b 1
    )
)

if not exist "%NVCLOTH_PXSHARED_DIR%\include\foundation\Px.h" (
    call :Error "NvCloth PxShared submodule is missing. Expected %NVCLOTH_PXSHARED_DIR%\include\foundation\Px.h."
    exit /b 1
)

call :ApplyPatches
if errorlevel 1 exit /b 1

call :HasStagedArtifacts
if not errorlevel 1 (
    call :AskRebuild
    if errorlevel 2 (
        echo [NvCloth] Skipping NvCloth rebuild; staged artifacts are already available.
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

call :HasStagedArtifacts
if errorlevel 1 (
    call :Error "NvCloth staged artifact verification failed."
    exit /b 1
)

echo [NvCloth] NvCloth is ready at "%NVCLOTH_STAGE_ROOT%".
exit /b 0

:HasStagedArtifacts
call :RequireFile "%NVCLOTH_STAGE_ROOT%\Include\NvCloth\Factory.h"
if errorlevel 1 exit /b 1

call :RequireFile "%NVCLOTH_STAGE_ROOT%\Include\NvClothExt\ClothFabricCooker.h"
if errorlevel 1 exit /b 1

call :RequireFile "%NVCLOTH_STAGE_ROOT%\Include\PxShared\foundation\PxVec3.h"
if errorlevel 1 exit /b 1

call :RequireFile "%NVCLOTH_STAGE_ROOT%\Lib\x64\Debug\NvClothDEBUG_x64.lib"
if errorlevel 1 exit /b 1
call :RequireFile "%NVCLOTH_STAGE_ROOT%\Bin\x64\Debug\NvClothDEBUG_x64.dll"
if errorlevel 1 exit /b 1
call :RequireFile "%NVCLOTH_STAGE_ROOT%\Lib\x64\Release\NvCloth_x64.lib"
if errorlevel 1 exit /b 1
call :RequireFile "%NVCLOTH_STAGE_ROOT%\Bin\x64\Release\NvCloth_x64.dll"
if errorlevel 1 exit /b 1

exit /b 0

:RequireFile
if exist "%~1" exit /b 0
exit /b 1

:AskRebuild
if /I "%NVCLOTH_REBUILD%"=="Y" exit /b 1
if /I "%NVCLOTH_REBUILD%"=="YES" exit /b 1
if /I "%NVCLOTH_REBUILD%"=="1" exit /b 1
if /I "%NVCLOTH_REBUILD%"=="N" exit /b 2
if /I "%NVCLOTH_REBUILD%"=="NO" exit /b 2
if /I "%NVCLOTH_REBUILD%"=="0" exit /b 2

echo [NvCloth] Existing staged NvCloth artifacts were found.
set "REBUILD_ANSWER="
set /p "REBUILD_ANSWER=[NvCloth] Rebuild NvCloth anyway? [Y/N]: "
if /I "%REBUILD_ANSWER%"=="Y" exit /b 1
if /I "%REBUILD_ANSWER%"=="YES" exit /b 1
exit /b 2

:ApplyPatches
echo [NvCloth] Applying NvCloth compatibility patches...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%REPO_ROOT%\Scripts\NvCloth\ApplyNvClothPatches.ps1" -NvClothSdkDir "%NVCLOTH_SDK_DIR%"
if errorlevel 1 (
    call :Error "Failed to apply NvCloth patches."
    exit /b 1
)
exit /b 0

:ResolveCMake
where /q cmake
if errorlevel 1 (
    call :Error "cmake.exe was not found. Install CMake 3.5 or newer or add it to PATH."
    exit /b 1
)

set "CMAKE_EXE=cmake"
exit /b 0

:GenerateProjects
echo [NvCloth] Generating NvCloth projects with CUDA disabled and DX11 enabled...
set "GW_DEPS_ROOT=%NVCLOTH_ROOT%"

"%CMAKE_EXE%" -Wno-dev -Wno-deprecated -S "%NVCLOTH_SDK_DIR%\compiler\cmake\windows" -B "%NVCLOTH_BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 -T v143 "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" -USTATIC_WINCRT -DTARGET_BUILD_PLATFORM=windows -DNV_CLOTH_ENABLE_CUDA=0 -DNV_CLOTH_ENABLE_DX11=1 -DPX_GENERATE_GPU_PROJECTS=0 "-DPX_OUTPUT_DLL_DIR=%NVCLOTH_OUTPUT_DIR%" "-DPX_OUTPUT_LIB_DIR=%NVCLOTH_OUTPUT_DIR%" "-DPX_OUTPUT_EXE_DIR=%NVCLOTH_OUTPUT_DIR%"
if errorlevel 1 (
    call :Error "NvCloth project generation failed."
    exit /b 1
)

if not exist "%NVCLOTH_BUILD_DIR%\" (
    call :Error "NvCloth project generation did not create %NVCLOTH_BUILD_DIR%."
    exit /b 1
)

exit /b 0

:BuildConfig
set "BUILD_CONFIG=%~1"

echo [NvCloth] Building NvCloth %BUILD_CONFIG%...
"%CMAKE_EXE%" --build "%NVCLOTH_BUILD_DIR%" --config "%BUILD_CONFIG%" -- /m
if errorlevel 1 (
    call :Error "NvCloth %BUILD_CONFIG% build failed."
    exit /b 1
)

exit /b 0

:StageIncludes
echo [NvCloth] Staging NvCloth headers...

if not exist "%NVCLOTH_SDK_DIR%\include\" (
    call :Error "Missing NvCloth include directory: %NVCLOTH_SDK_DIR%\include."
    exit /b 1
)

if not exist "%NVCLOTH_SDK_DIR%\extensions\include\" (
    call :Error "Missing NvCloth extension include directory: %NVCLOTH_SDK_DIR%\extensions\include."
    exit /b 1
)

if not exist "%NVCLOTH_PXSHARED_DIR%\include\" (
    call :Error "Missing NvCloth PxShared include directory: %NVCLOTH_PXSHARED_DIR%\include."
    exit /b 1
)

call :RobocopyDir "%NVCLOTH_SDK_DIR%\include\NvCloth" "%NVCLOTH_STAGE_ROOT%\Include\NvCloth"
if errorlevel 1 exit /b 1

call :RobocopyDir "%NVCLOTH_SDK_DIR%\extensions\include\NvClothExt" "%NVCLOTH_STAGE_ROOT%\Include\NvClothExt"
if errorlevel 1 exit /b 1

call :RobocopyDir "%NVCLOTH_PXSHARED_DIR%\include" "%NVCLOTH_STAGE_ROOT%\Include\PxShared"
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

:FindNvClothArtifactDir
set "NVCLOTH_ARTIFACT_DIR="
set "NVCLOTH_BIN_SEARCH_ROOT=%NVCLOTH_SDK_DIR%\bin"

if not exist "%NVCLOTH_BIN_SEARCH_ROOT%\" (
    call :Error "NvCloth bin directory was not found: %NVCLOTH_BIN_SEARCH_ROOT%."
    exit /b 1
)

for /f "delims=" %%D in ('dir /b /ad /o-d "%NVCLOTH_BIN_SEARCH_ROOT%\win.x86_64.*.md" 2^>nul') do (
    if not defined NVCLOTH_ARTIFACT_DIR (
        if exist "%NVCLOTH_BIN_SEARCH_ROOT%\%%D\%NVCLOTH_LIB_FILE%" (
            set "NVCLOTH_ARTIFACT_DIR=%NVCLOTH_BIN_SEARCH_ROOT%\%%D"
        )
    )
)

if not defined NVCLOTH_ARTIFACT_DIR (
    call :Error "No NvCloth artifact directory containing %NVCLOTH_LIB_FILE% was found under %NVCLOTH_BIN_SEARCH_ROOT%\win.x86_64.*.md."
    exit /b 1
)

echo [NvCloth] Using NvCloth artifacts from "%NVCLOTH_ARTIFACT_DIR%".
exit /b 0

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

exit /b 0

:CopyOptionalFile
set "COPY_SRC=%~1"
set "COPY_DST=%~2"

if not exist "%COPY_SRC%" exit /b 0

for %%D in ("%COPY_DST%") do (
    if not exist "%%~dpD" mkdir "%%~dpD"
)

copy /Y "%COPY_SRC%" "%COPY_DST%" >nul
if errorlevel 1 (
    call :Error "Failed to copy optional file %COPY_SRC% to %COPY_DST%."
    exit /b 1
)

exit /b 0

:StageConfig
set "STAGE_CONFIG=%~1"
set "LIB_DEST=%NVCLOTH_STAGE_ROOT%\Lib\x64\%STAGE_CONFIG%"
set "BIN_DEST=%NVCLOTH_STAGE_ROOT%\Bin\x64\%STAGE_CONFIG%"

set "NVCLOTH_LIB_FILE=NvCloth_x64.lib"
set "NVCLOTH_DLL_FILE=NvCloth_x64.dll"
set "NVCLOTH_PDB_FILE=NvCloth_x64.pdb"
if /I "%STAGE_CONFIG%"=="Debug" (
    set "NVCLOTH_LIB_FILE=NvClothDEBUG_x64.lib"
    set "NVCLOTH_DLL_FILE=NvClothDEBUG_x64.dll"
    set "NVCLOTH_PDB_FILE=NvClothDEBUG_x64.pdb"
)

echo [NvCloth] Staging NvCloth %STAGE_CONFIG% libs and DLLs...

if not exist "%LIB_DEST%\" mkdir "%LIB_DEST%"
if not exist "%BIN_DEST%\" mkdir "%BIN_DEST%"

call :FindNvClothArtifactDir
if errorlevel 1 exit /b 1

call :CopyFile "%NVCLOTH_ARTIFACT_DIR%\%NVCLOTH_LIB_FILE%" "%LIB_DEST%\%NVCLOTH_LIB_FILE%"
if errorlevel 1 exit /b 1

call :CopyFile "%NVCLOTH_ARTIFACT_DIR%\%NVCLOTH_DLL_FILE%" "%BIN_DEST%\%NVCLOTH_DLL_FILE%"
if errorlevel 1 exit /b 1

call :CopyOptionalFile "%NVCLOTH_ARTIFACT_DIR%\%NVCLOTH_PDB_FILE%" "%BIN_DEST%\%NVCLOTH_PDB_FILE%"
if errorlevel 1 exit /b 1

exit /b 0

:Error
echo [NvCloth] ERROR: %~1
exit /b 1
