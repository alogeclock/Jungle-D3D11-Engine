@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "LOCAL_PYTHON=%~dp0python\python.exe"
if exist "%LOCAL_PYTHON%" (
	"%LOCAL_PYTHON%" %*
	exit /b !ERRORLEVEL!
)

where py.exe >nul 2>&1
if not errorlevel 1 (
	py -3 %*
	exit /b !ERRORLEVEL!
)

where python.exe >nul 2>&1
if not errorlevel 1 (
	python %*
	exit /b !ERRORLEVEL!
)

echo [ERROR] Python 3 was not found.
echo Install Python 3 or place python.exe in "%~dp0python".
exit /b 1
