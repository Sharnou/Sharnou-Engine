$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build/vs2022-release"
$limit = 1024MB

$files = Get-ChildItem -Path $build -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match "\\Release\\" -and $_.Extension -in @(".exe",".dll") }

$total = ($files | Measure-Object -Property Length -Sum).Sum
if (-not $total) { $total = 0 }

Write-Host ("Runtime binary size: {0:N2} MiB" -f ($total / 1MB))
if ($total -ge $limit) {
    throw "Runtime binaries exceed the 1 GiB package guard."
}
