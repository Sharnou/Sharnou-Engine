[CmdletBinding()]
param(
    [string] $PinFile = ''
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($PinFile)) {
    $search = [System.IO.DirectoryInfo]::new([System.IO.Path]::GetFullPath($PSScriptRoot))
    while ($null -ne $search -and -not (Test-Path -LiteralPath (Join-Path $search.FullName 'compiler-version.json') -PathType Leaf)) {
        $search = $search.Parent
    }
    if ($null -eq $search) { throw 'Could not locate compiler-version.json above the resolver script.' }
    $PinFile = Join-Path $search.FullName 'compiler-version.json'
}
$pin = Get-Content -LiteralPath $PinFile -Raw | ConvertFrom-Json
$repo = [string]$pin.compiler_repository
$tag = [string]$pin.release_tag
$assetName = [string]$pin.asset_name
$expectedHash = ([string]$pin.asset_sha256).ToUpperInvariant()
$token = [string]$env:SHARNOU_COMPILER_READ_TOKEN
if ([string]::IsNullOrWhiteSpace($token)) {
    throw 'Set SHARNOU_COMPILER_READ_TOKEN to a fine-grained read-only token for the private Sharnou/Sharnou-Compiler repository.'
}
if ($repo -cne 'Sharnou/Sharnou-Compiler') { throw 'Unexpected compiler repository in pin file.' }

$headers = @{
    Authorization = "Bearer $token"
    Accept = 'application/vnd.github+json'
    'X-GitHub-Api-Version' = '2022-11-28'
}
$releaseUri = "https://api.github.com/repos/$repo/releases/tags/$tag"
$release = Invoke-RestMethod -Uri $releaseUri -Headers $headers -Method Get
if ([string]$release.tag_name -cne $tag) { throw 'GitHub returned a different release tag than the pin.' }

$requiredNames = @($assetName, ($assetName + '.sha256'), 'release-manifest.json', 'CHANGELOG.md')
foreach ($requiredName in $requiredNames) {
        $assetMatches = @($release.assets | Where-Object { $_.name -ceq $requiredName })
    if ($assetMatches.Count -ne 1) {
        throw "Pinned release is missing or duplicates required asset '$requiredName'."
    }
}
$zipAsset = @($release.assets | Where-Object { $_.name -ceq $assetName })[0]
$shaAsset = @($release.assets | Where-Object { $_.name -ceq ($assetName + '.sha256') })[0]

$cacheRoot = Join-Path $env:LOCALAPPDATA (Join-Path 'Sharnou\Compiler' $tag)
New-Item -ItemType Directory -Path $cacheRoot -Force | Out-Null
$zipPath = Join-Path $cacheRoot $assetName
if (Test-Path -LiteralPath $zipPath -PathType Leaf) {
    $cachedHash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToUpperInvariant()
    if ($cachedHash -cne $expectedHash) { Remove-Item -LiteralPath $zipPath -Force }
}
if (-not (Test-Path -LiteralPath $zipPath -PathType Leaf)) {
    $downloadHeaders = @{
        Authorization = "Bearer $token"
        Accept = 'application/octet-stream'
        'X-GitHub-Api-Version' = '2022-11-28'
    }
    Invoke-WebRequest -Uri ([string]$zipAsset.url) -Headers $downloadHeaders -OutFile $zipPath -UseBasicParsing
}
$actualHash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToUpperInvariant()
if ($actualHash -cne $expectedHash) {
    Remove-Item -LiteralPath $zipPath -Force
    throw "Downloaded compiler archive SHA-256 mismatch. Expected $expectedHash; actual $actualHash."
}

$sidecarPath = Join-Path $cacheRoot ($assetName + '.sha256')
$sidecarHeaders = @{
    Authorization = "Bearer $token"
    Accept = 'application/octet-stream'
    'X-GitHub-Api-Version' = '2022-11-28'
}
Invoke-WebRequest -Uri ([string]$shaAsset.url) -Headers $sidecarHeaders -OutFile $sidecarPath -UseBasicParsing
$sidecar = (Get-Content -LiteralPath $sidecarPath -Raw).Trim()
if ($sidecar -notmatch ('^' + [regex]::Escape($expectedHash) + '\s+' + [regex]::Escape($assetName) + '$')) {
    throw 'Pinned release checksum asset does not match compiler-version.json.'
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
try {
    if ($zip.Entries.Count -ne [int]$pin.archive_entries) {
        throw ("Pinned archive entry count mismatch. Expected {0}; actual {1}." -f $pin.archive_entries, $zip.Entries.Count)
    }
}
finally { $zip.Dispose() }

$packageRoot = Join-Path $cacheRoot 'package'
$bootstrapPath = Join-Path $packageRoot ([string]$pin.bootstrap_compiler.path)
$needsExtract = -not (Test-Path -LiteralPath $bootstrapPath -PathType Leaf)
if (-not $needsExtract) {
    $existingCompilerHash = (Get-FileHash -LiteralPath $bootstrapPath -Algorithm SHA256).Hash.ToUpperInvariant()
    if ($existingCompilerHash -cne ([string]$pin.bootstrap_compiler.sha256).ToUpperInvariant()) { $needsExtract = $true }
}
if ($needsExtract) {
    if (Test-Path -LiteralPath $packageRoot) { Remove-Item -LiteralPath $packageRoot -Recurse -Force }
    New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
    Expand-Archive -LiteralPath $zipPath -DestinationPath $packageRoot -Force
}
$bootstrapPath = Join-Path $packageRoot ([string]$pin.bootstrap_compiler.path)
if (-not (Test-Path -LiteralPath $bootstrapPath -PathType Leaf)) { throw "Bootstrap compiler missing after extraction: $bootstrapPath" }
$bootstrapHash = (Get-FileHash -LiteralPath $bootstrapPath -Algorithm SHA256).Hash.ToUpperInvariant()
if ($bootstrapHash -cne ([string]$pin.bootstrap_compiler.sha256).ToUpperInvariant()) {
    throw "Bootstrap compiler hash mismatch after extraction. Expected $($pin.bootstrap_compiler.sha256); actual $bootstrapHash."
}

Write-Host "PASS: pinned Sharnou compiler release $tag resolved and SHA-256 verified." -ForegroundColor Green
Write-Host "Archive SHA-256: $actualHash"
Write-Host "Bootstrap compiler SHA-256: $bootstrapHash"
Write-Host "Bootstrap source language: $($pin.bootstrap_compiler.source_language)"
if (-not [bool]$pin.bootstrap_compiler.native_cpp_frontend) {
    Write-Warning 'This pinned release is not a native C++ compiler and cannot compile .cpp/.hpp targets.'
}
[pscustomobject]@{
    ReleaseTag = $tag
    ArchivePath = $zipPath
    PackageRoot = $packageRoot
    BootstrapCompiler = $bootstrapPath
    ArchiveSha256 = $actualHash
    BootstrapSha256 = $bootstrapHash
    NativeCppFrontend = [bool]$pin.bootstrap_compiler.native_cpp_frontend
    SourceLanguage = [string]$pin.bootstrap_compiler.source_language
}
