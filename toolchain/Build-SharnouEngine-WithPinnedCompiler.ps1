[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$resolved = & (Join-Path $PSScriptRoot 'Resolve-PinnedSharnouCompiler.ps1')
if (-not $resolved -or $resolved.ReleaseTag -ne 'R26.0-R2-R8') { throw 'Pinned compiler resolution did not return the expected release.' }

$cppSources = @(Get-ChildItem -LiteralPath (Join-Path $root 'src') -Recurse -File | Where-Object { $_.Extension -in @('.cpp','.cc','.cxx','.hpp','.h') })
if ($cppSources.Count -gt 0 -and -not $resolved.NativeCppFrontend) {
    $examples = @($cppSources | Select-Object -First 5 -ExpandProperty FullName)
    Write-Error ("BUILD BLOCKED: pinned compiler R26.0-R2-R8 accepts .sharnou and has no C++ frontend/native sharnou-cxx.exe. Engine has {0} C++ source/header files, including: {1}. No fallback compiler is permitted." -f $cppSources.Count, ($examples -join '; '))
    exit 2
}
throw 'The compiler capability gate passed, but no native Engine build adapter is configured for this release.'
