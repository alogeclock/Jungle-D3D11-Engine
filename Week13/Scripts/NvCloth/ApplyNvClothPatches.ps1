param(
    [Parameter(Mandatory = $true)]
    [string]$NvClothSdkDir
)

$ErrorActionPreference = "Stop"
$Utf8NoBom = [System.Text.UTF8Encoding]::new($false)

function Update-TextFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [scriptblock]$Update
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Required file is missing: $Path"
    }

    $text = [System.IO.File]::ReadAllText($Path)
    $updated = & $Update $text

    if ($updated -ne $text) {
        [System.IO.File]::WriteAllText($Path, $updated, $Utf8NoBom)
        Write-Host "[NvCloth] Patched $Path"
    }
}

$allocatorHeader = Join-Path $NvClothSdkDir "include\NvCloth\ps\PsAllocator.h"
Update-TextFile -Path $allocatorHeader -Update {
    param($text)
    $text.Replace("#include <typeinfo.h>", "#include <typeinfo>")
}

$windowsCmake = Join-Path $NvClothSdkDir "compiler\cmake\windows\NvCloth.cmake"
$cudaLinkPattern = '(?s)TARGET_LINK_LIBRARIES\(NvCloth PUBLIC \$\{CUDA_CUDA_LIBRARY\}\)\s*SET_TARGET_PROPERTIES\(NvCloth PROPERTIES\s+LINK_FLAGS_DEBUG "/DELAYLOAD:nvcuda\.dll /MAP  /DEBUG"\s+LINK_FLAGS_CHECKED "/DELAYLOAD:nvcuda\.dll /MAP "\s+LINK_FLAGS_PROFILE "/DELAYLOAD:nvcuda\.dll /MAP /INCREMENTAL:NO /DEBUG"\s+LINK_FLAGS_RELEASE "/DELAYLOAD:nvcuda\.dll /MAP /INCREMENTAL:NO"\s*\)'
$cudaLinkReplacement = @'
IF(${NV_CLOTH_ENABLE_CUDA})
TARGET_LINK_LIBRARIES(NvCloth PUBLIC ${CUDA_CUDA_LIBRARY})
SET_TARGET_PROPERTIES(NvCloth PROPERTIES
	LINK_FLAGS_DEBUG "/DELAYLOAD:nvcuda.dll /MAP  /DEBUG"
	LINK_FLAGS_CHECKED "/DELAYLOAD:nvcuda.dll /MAP "
	LINK_FLAGS_PROFILE "/DELAYLOAD:nvcuda.dll /MAP /INCREMENTAL:NO /DEBUG"
	LINK_FLAGS_RELEASE "/DELAYLOAD:nvcuda.dll /MAP /INCREMENTAL:NO"
)
ELSE()
SET_TARGET_PROPERTIES(NvCloth PROPERTIES
	LINK_FLAGS_DEBUG "/MAP  /DEBUG"
	LINK_FLAGS_CHECKED "/MAP "
	LINK_FLAGS_PROFILE "/MAP /INCREMENTAL:NO /DEBUG"
	LINK_FLAGS_RELEASE "/MAP /INCREMENTAL:NO"
)
ENDIF()
'@

Update-TextFile -Path $windowsCmake -Update {
    param($text)

    if ($text -notmatch "DELAYLOAD:nvcuda\.dll") {
        return $text
    }

    if ($text -match "IF\(\$\{NV_CLOTH_ENABLE_CUDA\}\)\s*TARGET_LINK_LIBRARIES\(NvCloth PUBLIC \$\{CUDA_CUDA_LIBRARY\}\)") {
        return $text
    }

    $updated = [regex]::Replace($text, $cudaLinkPattern, { param($match) $cudaLinkReplacement }, 1)
    if ($updated -eq $text) {
        throw "Unable to patch CUDA linker flags in $windowsCmake"
    }

    $updated
}
