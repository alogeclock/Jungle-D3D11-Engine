param(
    [Parameter(Mandatory = $true)]
    [string]$RepoRoot
)

$ErrorActionPreference = "Stop"

$PythonVersion = "3.12.4"
$PythonUrl = "https://www.python.org/ftp/python/$PythonVersion/python-$PythonVersion-embed-amd64.zip"
$CMakeVersion = "3.31.6"
$CMakeUrl = "https://github.com/Kitware/CMake/releases/download/v$CMakeVersion/cmake-$CMakeVersion-windows-x86_64.zip"

$repoRootPath = [System.IO.Path]::GetFullPath($RepoRoot)
$scriptsDir = Join-Path $repoRootPath "Scripts"
$pythonDir = Join-Path $scriptsDir "python"
$pythonExe = Join-Path $pythonDir "python.exe"
$toolsDir = Join-Path $repoRootPath "KraftonEngine\Build\Tools"
$downloadsDir = Join-Path $toolsDir "Downloads"
$cmakeDir = Join-Path $toolsDir "CMake"
$cmakeExe = Join-Path $cmakeDir "bin\cmake.exe"

function Download-File {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Url,
        [Parameter(Mandatory = $true)]
        [string]$Destination
    )

    New-Item -ItemType Directory -Force -Path ([System.IO.Path]::GetDirectoryName($Destination)) | Out-Null
    Write-Host "[Tools] Downloading $Url"
    Invoke-WebRequest -Uri $Url -OutFile $Destination
}

function Ensure-EmbeddedPython {
    if (Test-Path -LiteralPath $pythonExe) {
        Write-Host "[Tools] Python found at `"$pythonExe`"."
        return
    }

    $zipPath = Join-Path $downloadsDir "python-$PythonVersion-embed-amd64.zip"
    Download-File -Url $PythonUrl -Destination $zipPath

    New-Item -ItemType Directory -Force -Path $pythonDir | Out-Null
    Expand-Archive -Path $zipPath -DestinationPath $pythonDir -Force

    if (-not (Test-Path -LiteralPath $pythonExe)) {
        throw "Python install failed. Expected $pythonExe"
    }

    Write-Host "[Tools] Python installed at `"$pythonExe`"."
}

function Ensure-CMake {
    if (Test-Path -LiteralPath $cmakeExe) {
        Write-Host "[Tools] CMake found at `"$cmakeExe`"."
        return
    }

    $pathCMake = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($pathCMake) {
        Write-Host "[Tools] CMake found on PATH at `"$($pathCMake.Source)`"."
        return
    }

    $zipPath = Join-Path $downloadsDir "cmake-$CMakeVersion-windows-x86_64.zip"
    $extractDir = Join-Path $downloadsDir "cmake-$CMakeVersion-extract"
    Download-File -Url $CMakeUrl -Destination $zipPath

    New-Item -ItemType Directory -Force -Path $extractDir | Out-Null
    Expand-Archive -Path $zipPath -DestinationPath $extractDir -Force

    $extractedRoot = Get-ChildItem -LiteralPath $extractDir -Directory |
        Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "bin\cmake.exe") } |
        Select-Object -First 1
    if (-not $extractedRoot) {
        throw "CMake archive layout was not recognized."
    }

    New-Item -ItemType Directory -Force -Path $cmakeDir | Out-Null
    Copy-Item -LiteralPath (Join-Path $extractedRoot.FullName "*") -Destination $cmakeDir -Recurse -Force

    if (-not (Test-Path -LiteralPath $cmakeExe)) {
        throw "CMake install failed. Expected $cmakeExe"
    }

    Write-Host "[Tools] CMake installed at `"$cmakeExe`"."
}

Ensure-EmbeddedPython
Ensure-CMake
