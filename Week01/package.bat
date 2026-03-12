@echo off
setlocal

set "PROJECT_DIR=%~dp0"
set "SLN_PATH=%PROJECT_DIR%Jungle_Week1_Team2.sln"
set "OUTPUT_DIR=%PROJECT_DIR%Build_Release"
set "EXE_PATH=%PROJECT_DIR%x64\Release\Jungle_Week1_Team2.exe"

echo.
echo ========================================
echo  Rhythm Jungle - Build ^& Package
echo ========================================
echo.

:: -----------------------------------------------
:: 1. MSBuild 경로 탐색
:: -----------------------------------------------
echo [1/4] MSBuild 탐색 중...

set "MSBUILD="

:: vswhere로 최신 VS 설치 경로 탐색
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2^>nul`) do (
    set "MSBUILD=%%i"
)

if not defined MSBUILD (
    echo [ERROR] MSBuild를 찾을 수 없습니다.
    echo         Visual Studio가 설치되어 있는지 확인하세요.
    pause
    exit /b 1
)

echo         MSBuild: %MSBUILD%
echo.

:: -----------------------------------------------
:: 2. Release|x64 빌드
:: -----------------------------------------------
echo [2/4] Release^|x64 빌드 중...
echo.

"%MSBUILD%" "%SLN_PATH%" /p:Configuration=Release /p:Platform=x64 /m /v:minimal

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] 빌드 실패! 에러를 확인하세요.
    pause
    exit /b 1
)

echo.
echo         빌드 성공!
echo.

:: -----------------------------------------------
:: 3. exe 존재 확인
:: -----------------------------------------------
echo [3/4] 빌드 결과 확인 중...

if not exist "%EXE_PATH%" (
    echo [ERROR] exe 파일을 찾을 수 없습니다: %EXE_PATH%
    pause
    exit /b 1
)

echo         exe 확인: Jungle_Week1_Team2.exe
echo.

:: -----------------------------------------------
:: 4. 패키징
:: -----------------------------------------------
echo [4/4] 패키징 중...

if exist "%OUTPUT_DIR%" (
    echo         이전 빌드 폴더 정리...
    rmdir /s /q "%OUTPUT_DIR%"
)

mkdir "%OUTPUT_DIR%"

echo         exe 복사...
copy /y "%EXE_PATH%" "%OUTPUT_DIR%\Jungle_Week1_Team2.exe" >nul

echo         Resources 복사...
xcopy /e /i /q /y "%PROJECT_DIR%Resources" "%OUTPUT_DIR%\Resources" >nul

echo.
echo ========================================
echo  패키징 완료!
echo  출력 경로: %OUTPUT_DIR%
echo ========================================
echo.
pause
