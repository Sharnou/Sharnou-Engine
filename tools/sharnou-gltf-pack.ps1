param(
    [Parameter(Mandatory=$true)][ValidateSet("compress","decompress")][string]$Mode,
    [Parameter(Mandatory=$true)][string]$Path
)

$ErrorActionPreference = "Stop"
$inputPath = [System.IO.Path]::GetFullPath($Path)
if (-not [System.IO.File]::Exists($inputPath)) { throw "File not found: $inputPath" }

function Copy-Gzip([string]$Source,[string]$Destination,[bool]$Compress) {
    $sourceStream = $null
    $destinationStream = $null
    $codec = $null
    try {
        $sourceStream = [System.IO.File]::OpenRead($Source)
        $destinationStream = [System.IO.File]::Create($Destination)
        if ($Compress) {
            $codec = [System.IO.Compression.GZipStream]::new($destinationStream,[System.IO.Compression.CompressionLevel]::Optimal,$false)
            $sourceStream.CopyTo($codec)
        } else {
            $codec = [System.IO.Compression.GZipStream]::new($sourceStream,[System.IO.Compression.CompressionMode]::Decompress,$false)
            $codec.CopyTo($destinationStream)
        }
    } finally {
        if ($codec) { $codec.Dispose() }
        if ($sourceStream) { $sourceStream.Dispose() }
        if ($destinationStream) { $destinationStream.Dispose() }
    }
}

if ($Mode -eq "compress") {
    if ([System.IO.Path]::GetExtension($inputPath).ToLowerInvariant() -ne ".gltf") { throw "Compression input must be .gltf." }
    $outputPath = $inputPath + "z"
    Copy-Gzip -Source $inputPath -Destination $outputPath -Compress $true
    $original = (Get-Item -LiteralPath $inputPath).Length
    $compressed = (Get-Item -LiteralPath $outputPath).Length
    $saving = if ($original -gt 0) { [math]::Round((1 - ($compressed / $original)) * 100,2) } else { 0 }
    Write-Host "PASS: glTF packaged as $([System.IO.Path]::GetFileName($outputPath))"
    Write-Host "Original bytes: $original"
    Write-Host "Compressed bytes: $compressed"
    Write-Host "Space saved: $saving%"
} else {
    if ([System.IO.Path]::GetExtension($inputPath).ToLowerInvariant() -ne ".gltfz") { throw "Decompression input must be .gltfz." }
    $outputPath = $inputPath.Substring(0,$inputPath.Length-1)
    Copy-Gzip -Source $inputPath -Destination $outputPath -Compress $false
    Write-Host "PASS: glTF restored to $([System.IO.Path]::GetFileName($outputPath))"
}
