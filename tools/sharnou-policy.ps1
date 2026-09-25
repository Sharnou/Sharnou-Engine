$ErrorActionPreference = "Stop"
$root = [IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) ".."))
$forbiddenFiles = @("CMakeLists.txt","CMakePresets.json","vcpkg.json","*.sln","*.slnx","*.vcxproj","*.vcxproj.filters")
$violations = New-Object System.Collections.Generic.List[string]
foreach($pattern in $forbiddenFiles){
  Get-ChildItem -LiteralPath $root -Recurse -File -Force -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like $pattern -and $_.FullName -notmatch "[\\/]legacy[\\/]" } |
    ForEach-Object { $violations.Add("FORBIDDEN BUILD FILE: $($_.FullName.Substring($root.Length).TrimStart('\','/'))") }
}

$activeExtensions = @(".ps1",".cmd",".bat",".yml",".yaml",".json",".cpp",".c",".cc",".h",".hpp")
$forbiddenText = @(
  "(?i)\bmsbuild(\.exe)?\b",
  "(?i)\bdevenv(\.exe)?\b",
  "(?i)\bvcpkg(\.exe)?\b",
  "(?i)\bcmake(\.exe)?\b",
  "(?i)\bUnity(\.exe)?\b",
  "(?i)\bUnrealBuildTool(\.exe)?\b"
)
Get-ChildItem -LiteralPath $root -Recurse -File -Force -ErrorAction SilentlyContinue |
  Where-Object {
    $_.FullName -notmatch "[\\/]\.git[\\/]" -and
    $_.FullName -notmatch "[\\/]legacy[\\/]" -and
    $_.FullName -notmatch "[\\/]tools[\\/]sharnou-policy\.ps1$" -and
    $_.FullName -notmatch "[\\/]toolchain[\\/]sharnou-toolchain\.contract\.json$" -and
    $activeExtensions -contains $_.Extension.ToLowerInvariant()
  } |
  ForEach-Object {
    $path=$_.FullName; $content=Get-Content -LiteralPath $path -Raw
    foreach($pattern in $forbiddenText){ if($content -match $pattern){ $violations.Add("FORBIDDEN ACTIVE TOOL REFERENCE: $($path.Substring($root.Length).TrimStart('\','/')) -> $pattern") } }
  }

$textureDir = Join-Path $root "assets/textures"
if(Test-Path $textureDir){
  Get-ChildItem -LiteralPath $textureDir -Recurse -File -Force | ForEach-Object {
    if($_.Extension.ToLowerInvariant() -ne ".avif" -and $_.Name -notmatch "^README\.md$"){ $violations.Add("NON-AVIF TEXTURE: $($_.FullName.Substring($root.Length).TrimStart('\','/'))") }
  }
}

if($violations.Count -gt 0){ $violations | ForEach-Object { Write-Host $_ }; exit 1 }
Write-Host "PASS: Sharnou Engine is using the Sharnou-only authoring/runtime policy."
Write-Host "PASS: No Visual Studio/MSBuild/Windows SDK/CMake/vcpkg build path is active."
Write-Host "PASS: AVIF-only texture boundary is enforced."
