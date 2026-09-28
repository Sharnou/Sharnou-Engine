param(
    [string]$SourceRoot = (Join-Path $PSScriptRoot "..\src"),
    [string]$Output = (Join-Path $PSScriptRoot "..\Build\Runtime\SharnouEngine.exe"),
    [string]$Compiler = ""
)

$ErrorActionPreference = "Stop"
$repo = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))

Write-Host "=== Sharnou Engine self-contained build ===" -ForegroundColor Cyan
Write-Host "Project: honour-war"
Write-Host "Target: Windows 10 x64"
Write-Host "Engine: SharnouEngine"

$contract = Join-Path $PSScriptRoot "sharnou-toolchain.contract.json"
if (!(Test-Path $contract -PathType Leaf)) { throw "Missing Sharnou toolchain contract: $contract" }

& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "verify-sharnou-toolchain.ps1") -Compiler $Compiler
if ($LASTEXITCODE -ne 0) { throw "Self-contained Sharnou compiler verification failed: $LASTEXITCODE" }

if ([string]::IsNullOrWhiteSpace($Compiler)) {
    $candidates = @(
        (Join-Path $repo "toolchain\bin\sharnou-cxx.exe"),
        (Join-Path $repo "toolchain\bin\sharnoucc.exe"),
        (Join-Path $repo "Build\Toolchain\sharnou-cxx.exe"),
        (Join-Path $repo "Build\Toolchain\sharnoucc.exe")
    )
    $Compiler = $candidates | Where-Object { Test-Path $_ -PathType Leaf } | Select-Object -First 1
}

if (!$Compiler) {
    throw "Sharnou self-contained compiler is missing. Build is intentionally fail-closed; no Visual Studio, MSBuild, Windows SDK, CMake, vcpkg, Unity, Unreal, or external compiler download is permitted."
}

if (!(Test-Path $Compiler -PathType Leaf)) { throw "Compiler not found: $Compiler" }
if (!(Test-Path $SourceRoot -PathType Container)) { throw "Source root not found: $SourceRoot" }

$engineSource = Join-Path $SourceRoot "SharnouEngine.cpp"
if (!(Test-Path $engineSource -PathType Leaf)) { throw "Missing engine entry point: $engineSource" }

New-Item -ItemType Directory -Force (Split-Path $Output -Parent) | Out-Null

# The self-contained Sharnou compiler owns the command-line contract below.
& $Compiler `
    --target windows-x64 `
    --subsystem windows `
    --source-root $SourceRoot `
    --entry $engineSource `
    --output $Output

if ($LASTEXITCODE -ne 0) { throw "Sharnou compiler failed with exit code $LASTEXITCODE" }

if (!(Test-Path $Output -PathType Leaf)) { throw "Compiler reported success but produced no executable: $Output" }

Write-Host "PASS: $Output" -ForegroundColor Green
