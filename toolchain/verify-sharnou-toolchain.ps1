param(
    [string]$Compiler = ""
)

$ErrorActionPreference = "Stop"
$Root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))

Write-Host "=== Sharnou self-contained compiler verification ===" -ForegroundColor Cyan

$candidates = @()
if (![string]::IsNullOrWhiteSpace($Compiler)) {
    $candidates += [System.IO.Path]::GetFullPath($Compiler)
} else {
    $candidates += @(
        (Join-Path $Root "toolchain\bin\sharnou-cxx.exe"),
        (Join-Path $Root "toolchain\bin\sharnoucc.exe"),
        (Join-Path $Root "Build\Toolchain\sharnou-cxx.exe"),
        (Join-Path $Root "Build\Toolchain\sharnoucc.exe")
    )
}

$found = $candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1

if (!$found) {
    Write-Host "STATUS: MISSING" -ForegroundColor Yellow
    Write-Host "No self-contained Sharnou compiler is installed in an approved location."
    Write-Host "No external compiler will be downloaded."
    exit 2
}

$file = Get-Item -LiteralPath $found
$bytes = [System.IO.File]::ReadAllBytes($found)

if ($bytes.Length -lt 0x40 -or $bytes[0] -ne 0x4D -or $bytes[1] -ne 0x5A) {
    throw "Compiler is not a valid PE executable: $found"
}

$offset = [BitConverter]::ToInt32($bytes,0x3C)
if ($offset -lt 0 -or $offset + 6 -gt $bytes.Length) {
    throw "Compiler PE header is invalid: $found"
}

$machine = [BitConverter]::ToUInt16($bytes,$offset + 4)
if ($machine -ne 0x8664) {
    throw ("Sharnou compiler is not Windows x64. Machine=0x{0:X4}" -f $machine)
}

Write-Host "PASS: compiler found: $found" -ForegroundColor Green
Write-Host ("PASS: compiler size: {0:N0} bytes" -f $file.Length) -ForegroundColor Green
Write-Host ("PASS: compiler PE machine: 0x{0:X4}" -f $machine) -ForegroundColor Green
Write-Host ("SHA-256: {0}" -f (Get-FileHash -LiteralPath $found -Algorithm SHA256).Hash)
exit 0
