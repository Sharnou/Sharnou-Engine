param(
    [ValidateSet("Release","Debug")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

if (-not $env:VCPKG_ROOT) {
    throw "VCPKG_ROOT is not set. Install vcpkg and set VCPKG_ROOT first."
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake is not on PATH."
}

cmake --preset windows-msvc-release
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

cmake --build build/vs2022-release --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed." }

& "$PSScriptRoot/size_guard.ps1"
if ($LASTEXITCODE -ne 0) { throw "Size guard failed." }

Write-Host "Sharnou Engine build completed."
