$ErrorActionPreference = "Stop"
$root = [IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) ".."))
$forbiddenFiles = @("CMakeLists.txt","CMakePresets.json","vcpkg.json","*.sln","*.slnx","*.vcxproj","*.vcxproj.filters")
$violations = New-Object System.Collections.Generic.List[string]

foreach($pattern in $forbiddenFiles){
  Get-ChildItem -LiteralPath $root -Recurse -File -Force -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like $pattern -and $_.FullName -notmatch "[\\/]legacy[\\/]" } |
    ForEach-Object { $violations.Add("FORBIDDEN BUILD FILE: $($_.FullName.Substring($root.Length).TrimStart('\\','/'))") }
}

# Executable/build-capable text is scanned for active forbidden tool usage.
# JSON contracts intentionally describe rejected tools and are not executable tool usage.
$activeExtensions = @(".ps1",".cmd",".bat",".yml",".yaml",".cpp",".c",".cc",".h",".hpp")
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
    $activeExtensions -contains $_.Extension.ToLowerInvariant()
  } |
  ForEach-Object {
    $path=$_.FullName
    $content=Get-Content -LiteralPath $path -Raw
    foreach($pattern in $forbiddenText){
      if($content -match $pattern){
        $violations.Add("FORBIDDEN ACTIVE TOOL REFERENCE: $($path.Substring($root.Length).TrimStart('\\','/')) -> $pattern")
      }
    }
  }

# Runtime texture policy:
# - KTX2 is required for shipped 3D material textures.
# - AVIF is required for shipped 2D/UI raster imagery.
$textureDir = Join-Path $root "assets/textures"
if(Test-Path $textureDir){
  Get-ChildItem -LiteralPath $textureDir -Recurse -File -Force | ForEach-Object {
    if($_.Name -match "^README\.md$"){ return }
    $ext=$_.Extension.ToLowerInvariant()
    if($ext -notin @(".avif",".ktx2")){
      $violations.Add("REJECTED TEXTURE FORMAT: $($_.FullName.Substring($root.Length).TrimStart('\\','/'))")
    }
  }
}

# Runtime model policy:
# glTF/GLB are the approved shipped 3D scene/model containers.
# FBX/OBJ are authoring/interchange inputs, not runtime model containers.
$modelDir = Join-Path $root "assets/models"
if(Test-Path $modelDir){
  Get-ChildItem -LiteralPath $modelDir -Recurse -File -Force | ForEach-Object {
    if($_.Name -match "^README\.md$"){ return }
    $ext=$_.Extension.ToLowerInvariant()
    if($ext -notin @(".gltf",".glb")){
      $violations.Add("REJECTED RUNTIME MODEL FORMAT: $($_.FullName.Substring($root.Length).TrimStart('\\','/'))")
    }
  }
}

if($violations.Count -gt 0){ $violations | ForEach-Object { Write-Host $_ }; exit 1 }
Write-Host "PASS: Sharnou Engine is using the Sharnou-only authoring/runtime policy."
Write-Host "PASS: No Visual Studio/MSBuild/Windows SDK/CMake/vcpkg build path is active."
Write-Host "PASS: 3D textures use KTX2; 2D raster visuals use AVIF."
Write-Host "PASS: glTF/GLB are the approved runtime 3D containers."
Write-Host "PASS: Declarative JSON contracts are excluded from active-tool execution scanning."
