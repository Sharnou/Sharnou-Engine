$ErrorActionPreference = 'Stop'

$Repository = $env:TARGET_REPOSITORY
$ExpectedCommit = $env:TARGET_COMMIT
$Root = $env:GITHUB_WORKSPACE
$ReportDir = Join-Path $Root $env:REPORT_DIR
$JsonFile = Join-Path $ReportDir 'dependency-report.json'
$MarkdownFile = Join-Path $ReportDir 'dependency-report.md'
$IncludeFile = Join-Path $ReportDir 'source-include-report.txt'
$RuntimeFile = Join-Path $ReportDir 'runtime-assumptions-report.txt'

New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null

$Rows = [System.Collections.Generic.List[object]]::new()
$BlockingReasons = [System.Collections.Generic.List[string]]::new()
$LocalIncludes = [System.Collections.Generic.List[object]]::new()
$ExternalIncludes = [System.Collections.Generic.List[object]]::new()
$RuntimeReferences = [System.Collections.Generic.List[object]]::new()

function Add-BlockingReason {
    param([string]$Text)
    if (-not [string]::IsNullOrWhiteSpace($Text)) {
        if (-not ($BlockingReasons -contains $Text)) {
            [void]$BlockingReasons.Add($Text)
        }
    }
}

function Add-DependencyRow {
    param(
        [string]$Dependency,
        [string]$RequiredSourceFile,
        [string]$HeaderStatus,
        [string]$LibraryStatus,
        [string]$Version,
        [string]$Architecture,
        [string]$Path,
        [string]$Provenance,
        [string]$ApprovedStatus,
        [string]$BlockingReason,
        [string]$CiBuildStatus
    )
    [void]$Rows.Add([ordered]@{
        Dependency = $Dependency
        'Required source file' = $RequiredSourceFile
        'Header status' = $HeaderStatus
        'Library status' = $LibraryStatus
        Version = $Version
        Architecture = $Architecture
        Path = $Path
        Provenance = $Provenance
        'Approved status' = $ApprovedStatus
        'Blocking reason' = $BlockingReason
        'CI build status' = $CiBuildStatus
    })
}

function Get-FileVersionMetadata {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return 'UNKNOWN'
    }
    try {
        $version = [string](Get-Item -LiteralPath $Path -ErrorAction Stop).VersionInfo.FileVersion
        if ([string]::IsNullOrWhiteSpace($version)) {
            return 'UNKNOWN'
        }
        return $version
    }
    catch {
        return 'UNKNOWN'
    }
}

function Get-PeArchitecture {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return 'UNKNOWN'
    }
    try {
        $bytes = [IO.File]::ReadAllBytes($Path)
        if ($bytes.Length -lt 64) {
            return 'UNKNOWN'
        }
        if ([Text.Encoding]::ASCII.GetString($bytes, 0, 2) -ne 'MZ') {
            return 'NOT_PE'
        }
        $peOffset = [BitConverter]::ToInt32($bytes, 0x3C)
        if ($peOffset -lt 0 -or $peOffset + 6 -gt $bytes.Length) {
            return 'UNKNOWN'
        }
        if ([Text.Encoding]::ASCII.GetString($bytes, $peOffset, 4) -ne "PE`0`0") {
            return 'NOT_PE'
        }
        $machine = [BitConverter]::ToUInt16($bytes, $peOffset + 4)
        switch ($machine) {
            0x8664 { return 'x64_0x8664' }
            0x014c { return 'x86_0x014C' }
            0xAA64 { return 'ARM64_0xAA64' }
            default { return ('PE_MACHINE_0x{0:X4}' -f $machine) }
        }
    }
    catch {
        return 'UNKNOWN'
    }
}

function Get-RegistryStringValue {
    param(
        [string]$Path,
        [string]$Name
    )
    if (-not (Test-Path -LiteralPath $Path)) {
        return $null
    }
    try {
        $property = Get-ItemProperty -LiteralPath $Path -ErrorAction Stop
        return [string]$property.$Name
    }
    catch {
        return $null
    }
}

function Resolve-RepositoryLocalInclude {
    param(
        [string]$CurrentFile,
        [string]$IncludeName
    )
    $candidates = @(
        (Join-Path (Split-Path -Parent $CurrentFile) $IncludeName),
        (Join-Path (Join-Path $Root 'src') $IncludeName),
        (Join-Path $Root $IncludeName)
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return (Get-Item -LiteralPath $candidate).FullName
        }
    }
    return $null
}

function Get-IncludeGraph {
    param([string]$EntryFile)
    $seen = @{}
    $queue = New-Object System.Collections.Queue
    if (-not (Test-Path -LiteralPath $EntryFile -PathType Leaf)) {
        return @()
    }
    $queue.Enqueue((Get-Item -LiteralPath $EntryFile).FullName)
    while ($queue.Count -gt 0) {
        $current = [string]$queue.Dequeue()
        if ($seen.ContainsKey($current)) {
            continue
        }
        $seen[$current] = $true
        $lineNumber = 0
        $contentLines = @(Get-Content -LiteralPath $current -Encoding UTF8 -ErrorAction SilentlyContinue)
        foreach ($line in $contentLines) {
            $lineNumber++
            $quotedMatch = [regex]::Match($line, '^\s*#\s*include\s*"([^"]+)"')
            if ($quotedMatch.Success) {
                $includeName = $quotedMatch.Groups[1].Value
                $resolved = Resolve-RepositoryLocalInclude -CurrentFile $current -IncludeName $includeName
                $resolvedDisplay = 'NOT RESOLVED'
                $status = 'MISSING'
                if ($null -ne $resolved) {
                    $resolvedDisplay = $resolved.Substring($Root.Length).TrimStart('\')
                    $status = 'FOUND AND APPROVED'
                }
                [void]$LocalIncludes.Add([ordered]@{
                    File = $current.Substring($Root.Length).TrimStart('\')
                    Line = $lineNumber
                    Include = $includeName
                    Resolved = $resolvedDisplay
                    Status = $status
                })
                if ($null -ne $resolved) {
                    $queue.Enqueue($resolved)
                }
                else {
                    Add-BlockingReason "Missing repository-local include '$includeName'."
                }
                continue
            }
            $angleMatch = [regex]::Match($line, '^\s*#\s*include\s*<([^>]+)>')
            if ($angleMatch.Success) {
                [void]$ExternalIncludes.Add([ordered]@{
                    File = $current.Substring($Root.Length).TrimStart('\')
                    Line = $lineNumber
                    Include = $angleMatch.Groups[1].Value
                    Status = 'EXTERNAL HEADER'
                    Resolution = 'Conditional compiler include resolution NOT TESTED'
                })
            }
        }
    }
    return @($seen.Keys)
}

function Find-HeaderInRoots {
    param(
        [string]$RelativePath,
        [string[]]$Roots
    )
    $found = [System.Collections.Generic.List[string]]::new()
    foreach ($rootPath in $Roots) {
        $candidate = Join-Path $rootPath $RelativePath
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            [void]$found.Add((Get-Item -LiteralPath $candidate).FullName)
        }
    }
    return @($found | Sort-Object -Unique)
}

function Find-LibraryInRoots {
    param(
        [string]$LibraryName,
        [string[]]$Roots
    )
    $found = [System.Collections.Generic.List[string]]::new()
    foreach ($rootPath in $Roots) {
        $candidate = Join-Path $rootPath $LibraryName
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            [void]$found.Add((Get-Item -LiteralPath $candidate).FullName)
        }
    }
    return @($found | Sort-Object -Unique)
}

function Get-ApprovedLibavifFiles {
    param([string[]]$DependencyRoots)
    $approvedNames = @(
        'avif.lib',
        'libavif.lib',
        'avif.dll',
        'libavif.dll',
        'avif.a',
        'libavif.a'
    )
    $excludePattern = '\\(?:\.git|Build|assets|Content|node_modules|\.cache)(?:\\|$)'
    $headerCandidates = [System.Collections.Generic.List[string]]::new()
    $libraryCandidates = [System.Collections.Generic.List[string]]::new()
    foreach ($dependencyRoot in $DependencyRoots) {
        if (-not (Test-Path -LiteralPath $dependencyRoot -PathType Container)) {
            continue
        }
        $files = @(Get-ChildItem -LiteralPath $dependencyRoot -Recurse -File -Force -ErrorAction SilentlyContinue)
        foreach ($file in $files) {
            $fullName = $file.FullName
            if ($fullName -match $excludePattern) {
                continue
            }
            $normalized = $fullName.Replace('/', '\')
            $leafName = $file.Name
            if ($normalized.ToLowerInvariant().EndsWith('\avif\avif.h')) {
                [void]$headerCandidates.Add($fullName)
            }
            if ($approvedNames -contains $leafName) {
                [void]$libraryCandidates.Add($fullName)
            }
        }
    }
    return [ordered]@{
        Headers = @($headerCandidates | Sort-Object -Unique)
        Libraries = @($libraryCandidates | Sort-Object -Unique)
    }
}

function Get-NearestAvifLicenseEvidence {
    param([string[]]$DependencyRoots)
    $evidence = [System.Collections.Generic.List[string]]::new()
    $namePattern = '(?i)(?:^|[._-])(avif|libavif)(?:$|[._-])'
    foreach ($rootPath in $DependencyRoots) {
        if (-not (Test-Path -LiteralPath $rootPath -PathType Container)) {
            continue
        }
        $files = @(Get-ChildItem -LiteralPath $rootPath -Recurse -File -Force -ErrorAction SilentlyContinue)
        foreach ($file in $files) {
            $name = $file.Name
            if ($name -match '(?i)license|copying|notice|copyright') {
                if ($name -match $namePattern -or $name -match '(?i)license|copying|notice|copyright') {
                    [void]$evidence.Add($file.FullName)
                }
            }
        }
    }
    return @($evidence | Sort-Object -Unique)
}

$commit = (git rev-parse HEAD).Trim()
$os = Get-CimInstance Win32_OperatingSystem
$computer = Get-CimInstance Win32_ComputerSystem
$runnerImage = if ([string]::IsNullOrWhiteSpace($env:ImageOS)) { 'UNKNOWN' } else { $env:ImageOS }
$runnerImageVersion = if ([string]::IsNullOrWhiteSpace($env:ImageVersion)) { 'UNKNOWN' } else { $env:ImageVersion }
$runnerArchitecture = if ([string]::IsNullOrWhiteSpace($env:PROCESSOR_ARCHITECTURE)) { 'UNKNOWN' } else { $env:PROCESSOR_ARCHITECTURE }

if ($commit -ne $ExpectedCommit) {
    Add-BlockingReason "Checked-out commit '$commit' does not match expected commit '$ExpectedCommit'."
}

$clCandidates = @()
$clCommand = Get-Command cl.exe -ErrorAction SilentlyContinue
if ($null -ne $clCommand) {
    $clCandidates += $clCommand.Source
}

$linkCandidates = @()
$linkCommand = Get-Command link.exe -ErrorAction SilentlyContinue
if ($null -ne $linkCommand) {
    $linkCandidates += $linkCommand.Source
}

$VisualStudioRegistryRoots = @(
    'HKLM:\SOFTWARE\Microsoft\VisualStudio\Setup\Instances',
    'HKLM:\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\Setup\Instances'
)
$VisualStudioInstances = [System.Collections.Generic.List[object]]::new()
$MsvcToolsets = [System.Collections.Generic.List[object]]::new()

foreach ($registryRoot in $VisualStudioRegistryRoots) {
    if (-not (Test-Path -LiteralPath $registryRoot)) {
        continue
    }
    $instanceKeys = @(Get-ChildItem -LiteralPath $registryRoot -ErrorAction SilentlyContinue)
    foreach ($instanceKey in $instanceKeys) {
        try {
            $properties = Get-ItemProperty -LiteralPath $instanceKey.PSPath -ErrorAction Stop
            $installationPath = [string]$properties.installationPath
            $displayVersion = [string]$properties.catalog_productDisplayVersion
            if ([string]::IsNullOrWhiteSpace($displayVersion)) {
                $displayVersion = [string]$properties.catalog_productLineVersion
            }
            $instanceVersion = if ([string]::IsNullOrWhiteSpace($displayVersion)) { 'UNKNOWN' } else { $displayVersion }
            [void]$VisualStudioInstances.Add([ordered]@{
                RegistryRoot = $registryRoot
                Instance = $instanceKey.PSChildName
                InstallationPath = $installationPath
                Version = $instanceVersion
            })
            if (-not [string]::IsNullOrWhiteSpace($installationPath) -and (Test-Path -LiteralPath $installationPath -PathType Container)) {
                $msvcRoot = Join-Path $installationPath 'VC\Tools\MSVC'
                if (Test-Path -LiteralPath $msvcRoot -PathType Container) {
                    $toolsetDirectories = @(Get-ChildItem -LiteralPath $msvcRoot -Directory -ErrorAction SilentlyContinue)
                    foreach ($toolsetDirectory in $toolsetDirectories) {
                        $binDirectory = Join-Path $toolsetDirectory.FullName 'bin\Hostx64\x64'
                        $clPath = Join-Path $binDirectory 'cl.exe'
                        $linkPath = Join-Path $binDirectory 'link.exe'
                        if (Test-Path -LiteralPath $clPath -PathType Leaf) {
                            $clCandidates += (Get-Item -LiteralPath $clPath).FullName
                        }
                        if (Test-Path -LiteralPath $linkPath -PathType Leaf) {
                            $linkCandidates += (Get-Item -LiteralPath $linkPath).FullName
                        }
                        $includePath = Join-Path $toolsetDirectory.FullName 'include'
                        $libraryPath = Join-Path $toolsetDirectory.FullName 'lib\x64'
                        [void]$MsvcToolsets.Add([ordered]@{
                            InstallationPath = $installationPath
                            ToolsetVersion = $toolsetDirectory.Name
                            ToolsetRoot = $toolsetDirectory.FullName
                            IncludePath = $includePath
                            LibraryPath = $libraryPath
                            IncludeExists = Test-Path -LiteralPath $includePath -PathType Container
                            LibraryExists = Test-Path -LiteralPath $libraryPath -PathType Container
                        })
                    }
                }
            }
        }
        catch {
            continue
        }
    }
}

$clCandidates = @($clCandidates | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)
$linkCandidates = @($linkCandidates | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)
$clMetadata = @(
    foreach ($candidate in $clCandidates) {
        [ordered]@{
            Path = $candidate
            VersionMetadata = Get-FileVersionMetadata $candidate
            Architecture = Get-PeArchitecture $candidate
            Functionality = 'NOT TESTED'
        }
    }
)
$linkMetadata = @(
    foreach ($candidate in $linkCandidates) {
        [ordered]@{
            Path = $candidate
            VersionMetadata = Get-FileVersionMetadata $candidate
            Architecture = Get-PeArchitecture $candidate
            Functionality = 'NOT TESTED'
        }
    }
)

if ($clMetadata.Count -eq 0) {
    Add-DependencyRow 'MSVC cl.exe' 'src/SharnouEngine.cpp' 'UNKNOWN' 'NOT APPLICABLE' 'UNKNOWN' 'UNKNOWN' 'NOT FOUND' 'Registry/filesystem discovery only' 'MISSING' 'MSVC cl.exe was not discovered; compiler functionality is NOT TESTED.' 'NOT TESTED'
    Add-BlockingReason 'MSVC cl.exe is missing from PATH and discovered Visual Studio MSVC toolset locations.'
}
else {
    Add-DependencyRow 'MSVC cl.exe' 'src/SharnouEngine.cpp' 'NOT APPLICABLE' 'NOT APPLICABLE' (($clMetadata | ForEach-Object VersionMetadata) -join '; ') (($clMetadata | ForEach-Object Architecture) -join '; ') (($clMetadata | ForEach-Object Path) -join '; ') 'Registry/filesystem discovery only' 'FOUND — APPROVAL REQUIRED' 'cl.exe presence is evidence only; compiler functionality remains NOT TESTED and explicit approval is required.' 'NOT TESTED'
    Add-BlockingReason 'MSVC cl.exe was discovered but is not explicitly approved; compiler functionality remains NOT TESTED.'
}

if ($linkMetadata.Count -eq 0) {
    Add-DependencyRow 'MSVC link.exe' 'src/SharnouEngine.cpp' 'NOT APPLICABLE' 'NOT APPLICABLE' 'UNKNOWN' 'UNKNOWN' 'NOT FOUND' 'Registry/filesystem discovery only' 'MISSING' 'MSVC link.exe was not discovered; linker functionality is NOT TESTED.' 'NOT TESTED'
    Add-BlockingReason 'MSVC link.exe is missing from PATH and discovered Visual Studio MSVC toolset locations.'
}
else {
    Add-DependencyRow 'MSVC link.exe' 'src/SharnouEngine.cpp' 'NOT APPLICABLE' 'NOT APPLICABLE' (($linkMetadata | ForEach-Object VersionMetadata) -join '; ') (($linkMetadata | ForEach-Object Architecture) -join '; ') (($linkMetadata | ForEach-Object Path) -join '; ') 'Registry/filesystem discovery only' 'FOUND — APPROVAL REQUIRED' 'link.exe presence is evidence only; linker functionality remains NOT TESTED and explicit approval is required.' 'NOT TESTED'
    Add-BlockingReason 'MSVC link.exe was discovered but is not explicitly approved; linker functionality remains NOT TESTED.'
}

$kitsRoot = Get-RegistryStringValue 'HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots' 'KitsRoot10'
$SdkVersions = [System.Collections.Generic.List[object]]::new()
$SdkIncludeRoots = [System.Collections.Generic.List[string]]::new()
$SdkLibraryRoots = [System.Collections.Generic.List[string]]::new()
if ($kitsRoot -and (Test-Path -LiteralPath $kitsRoot -PathType Container)) {
    $includeRoot = Join-Path $kitsRoot 'Include'
    $libraryRoot = Join-Path $kitsRoot 'Lib'
    if (Test-Path -LiteralPath $includeRoot -PathType Container) {
        $versionDirectories = @(Get-ChildItem -LiteralPath $includeRoot -Directory -ErrorAction SilentlyContinue | Sort-Object Name)
        foreach ($versionDirectory in $versionDirectories) {
            [void]$SdkVersions.Add([ordered]@{
                Version = $versionDirectory.Name
                IncludeRoot = $versionDirectory.FullName
                LibraryRoot = Join-Path $libraryRoot $versionDirectory.Name
            })
        }
    }
}
foreach ($sdk in $SdkVersions) {
    $includeCandidates = @(
        (Join-Path $sdk.IncludeRoot 'um'),
        (Join-Path $sdk.IncludeRoot 'shared'),
        (Join-Path $sdk.IncludeRoot 'ucrt'),
        (Join-Path $sdk.IncludeRoot 'winrt')
    )
    foreach ($candidate in $includeCandidates) {
        if (Test-Path -LiteralPath $candidate -PathType Container) {
            [void]$SdkIncludeRoots.Add($candidate)
        }
    }
    $libraryCandidates = @(
        (Join-Path $sdk.LibraryRoot 'um\x64'),
        (Join-Path $sdk.LibraryRoot 'ucrt\x64')
    )
    foreach ($candidate in $libraryCandidates) {
        if (Test-Path -LiteralPath $candidate -PathType Container) {
            [void]$SdkLibraryRoots.Add($candidate)
        }
    }
}
$SdkIncludeRoots = @($SdkIncludeRoots | Sort-Object -Unique)
$SdkLibraryRoots = @($SdkLibraryRoots | Sort-Object -Unique)
$MsvcIncludeRoots = @($MsvcToolsets | Where-Object { $_.IncludeExists } | ForEach-Object { $_.IncludePath } | Sort-Object -Unique)
$MsvcLibraryRoots = @($MsvcToolsets | Where-Object { $_.LibraryExists } | ForEach-Object { $_.LibraryPath } | Sort-Object -Unique)

if ($kitsRoot -and $SdkIncludeRoots.Count -gt 0) {
    $sdkStatus = 'FOUND — APPROVAL REQUIRED'
    $sdkReason = 'Windows SDK include roots were discovered; explicit approval is required.'
}
else {
    $sdkStatus = 'MISSING'
    $sdkReason = 'Windows SDK KitsRoot10 or usable include roots were not discovered.'
    Add-BlockingReason 'Windows SDK include roots are missing.'
}

$HeaderRequirements = @(
    [ordered]@{ Name = 'windows.h'; Relative = 'windows.h' },
    [ordered]@{ Name = 'd3d11.h'; Relative = 'd3d11.h' },
    [ordered]@{ Name = 'd3dcompiler.h'; Relative = 'd3dcompiler.h' },
    [ordered]@{ Name = 'dxgi.h'; Relative = 'dxgi.h' },
    [ordered]@{ Name = 'DirectXMath.h'; Relative = 'DirectXMath.h' },
    [ordered]@{ Name = 'wrl/client.h'; Relative = 'wrl\client.h' }
)
foreach ($header in $HeaderRequirements) {
    $headerPaths = @(Find-HeaderInRoots -RelativePath $header.Relative -Roots ($SdkIncludeRoots + $MsvcIncludeRoots))
    if ($headerPaths.Count -gt 0) {
        Add-DependencyRow $header.Name 'src/SharnouEngine.cpp' 'FOUND' 'NOT APPLICABLE' 'KNOWN PER DISCOVERED ROOT' 'UNKNOWN' (($headerPaths | ForEach-Object { $_.Substring($Root.Length).TrimStart('\') }) -join '; ') 'Windows SDK/MSVC filesystem discovery' $sdkStatus $sdkReason 'NOT TESTED'
        if ($sdkStatus -eq 'FOUND — APPROVAL REQUIRED') {
            Add-BlockingReason "Approval required for $($header.Name)."
        }
    }
    else {
        Add-DependencyRow $header.Name 'src/SharnouEngine.cpp' 'MISSING' 'NOT APPLICABLE' 'UNKNOWN' 'UNKNOWN' 'NOT FOUND' 'Windows SDK/MSVC filesystem discovery' 'MISSING' "Required header $($header.Name) was not discovered in approved system include roots." 'NOT TESTED'
        Add-BlockingReason "Required header $($header.Name) is missing."
    }
}

$LibraryRequirements = @('d3d11.lib', 'dxgi.lib', 'd3dcompiler.lib', 'user32.lib')
foreach ($libraryName in $LibraryRequirements) {
    $libraryPaths = @(Find-LibraryInRoots -LibraryName $libraryName -Roots $SdkLibraryRoots)
    if ($libraryPaths.Count -gt 0) {
        Add-DependencyRow $libraryName 'src/SharnouEngine.cpp' 'NOT APPLICABLE' 'FOUND' 'KNOWN PER DISCOVERED SDK ROOT' 'x64' (($libraryPaths | ForEach-Object { $_.Substring($Root.Length).TrimStart('\') }) -join '; ') 'Windows SDK filesystem discovery' $sdkStatus $sdkReason 'NOT TESTED'
        if ($sdkStatus -eq 'FOUND — APPROVAL REQUIRED') {
            Add-BlockingReason "Approval required for $libraryName."
        }
    }
    else {
        Add-DependencyRow $libraryName 'src/SharnouEngine.cpp' 'NOT APPLICABLE' 'MISSING' 'UNKNOWN' 'x64' 'NOT FOUND' 'Windows SDK filesystem discovery' 'MISSING' "Required library $libraryName was not discovered in x64 SDK library roots." 'NOT TESTED'
        Add-BlockingReason "Required library $libraryName is missing."
    }
}

$EngineSource = Join-Path $Root 'src\SharnouEngine.cpp'
$RuntimeIntake = Join-Path $Root 'src\asset\RuntimeAssetIntake.hpp'
$Ktx2Container = Join-Path $Root 'src\asset\Ktx2Container.hpp'
if (-not (Test-Path -LiteralPath $EngineSource -PathType Leaf)) {
    Add-BlockingReason 'Missing src/SharnouEngine.cpp.'
}
if (-not (Test-Path -LiteralPath $RuntimeIntake -PathType Leaf)) {
    Add-BlockingReason 'Missing src/asset/RuntimeAssetIntake.hpp.'
}
else {
    Add-DependencyRow 'RuntimeAssetIntake.hpp' 'src/SharnouEngine.cpp' 'FOUND' 'NOT APPLICABLE' 'REPOSITORY COMMIT PINNED' 'SOURCE' 'src/asset/RuntimeAssetIntake.hpp' 'Repository-owned source at the pinned commit' 'FOUND AND APPROVED' 'Repository-owned file is present at the verified pinned commit.' 'NOT TESTED'
}
if (-not (Test-Path -LiteralPath $Ktx2Container -PathType Leaf)) {
    Add-BlockingReason 'Missing src/asset/Ktx2Container.hpp.'
}
else {
    Add-DependencyRow 'Ktx2Container.hpp' 'src/asset/RuntimeAssetIntake.hpp' 'FOUND' 'NOT APPLICABLE' 'REPOSITORY COMMIT PINNED' 'SOURCE' 'src/asset/Ktx2Container.hpp' 'Repository-owned source at the pinned commit' 'FOUND AND APPROVED' 'Repository-owned file is present at the verified pinned commit.' 'NOT TESTED'
}

$reachableFiles = @()
if (Test-Path -LiteralPath $EngineSource -PathType Leaf) {
    $reachableFiles = @(Get-IncludeGraph -EntryFile $EngineSource)
}

$ApprovedDependencyDirectoryNames = @(
    'third_party',
    'third-party',
    'vendor',
    'vendors',
    'external',
    'externals',
    'deps',
    'dependencies',
    'lib',
    'libs'
)
$ApprovedDependencyRoots = [System.Collections.Generic.List[string]]::new()
foreach ($name in $ApprovedDependencyDirectoryNames) {
    $candidate = Join-Path $Root $name
    if (Test-Path -LiteralPath $candidate -PathType Container) {
        [void]$ApprovedDependencyRoots.Add((Get-Item -LiteralPath $candidate).FullName)
    }
}
foreach ($relative in @('src\third_party', 'src\vendor')) {
    $candidate = Join-Path $Root $relative
    if (Test-Path -LiteralPath $candidate -PathType Container) {
        [void]$ApprovedDependencyRoots.Add((Get-Item -LiteralPath $candidate).FullName)
    }
}
$ApprovedDependencyRoots = @($ApprovedDependencyRoots | Sort-Object -Unique)

$libavifFiles = Get-ApprovedLibavifFiles -DependencyRoots $ApprovedDependencyRoots
$AvifHeaders = @($libavifFiles.Headers)
$AvifLibraries = @($libavifFiles.Libraries)
$LicenseEvidence = @(Get-NearestAvifLicenseEvidence -DependencyRoots $ApprovedDependencyRoots)

$AvifApiNames = @(
    'avifDecoderCreate',
    'avifDecoderSetIOFile',
    'avifDecoderParse',
    'avifDecoderNextImage',
    'avifRGBImageSetDefaults',
    'avifRGBImageAllocatePixels',
    'avifImageYUVToRGB',
    'avifRGBImageFreePixels',
    'avifDecoderDestroy',
    'avifResultToString'
)
$AvifVersion = 'UNKNOWN'
$AvifApiStatus = 'UNKNOWN'
if ($AvifHeaders.Count -gt 0) {
    $firstHeaderText = Get-Content -LiteralPath $AvifHeaders[0] -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
    if ($null -eq $firstHeaderText) {
        $firstHeaderText = ''
    }
    $versionMatch = [regex]::Match($firstHeaderText, '#\s*define\s+AVIF_VERSION_STRING\s+"([^"]+)"')
    if ($versionMatch.Success) {
        $AvifVersion = $versionMatch.Groups[1].Value
    }
    $missingApiNames = @()
    foreach ($apiName in $AvifApiNames) {
        if ($firstHeaderText.IndexOf($apiName, [StringComparison]::Ordinal) -lt 0) {
            $missingApiNames += $apiName
        }
    }
    if ($missingApiNames.Count -eq 0) {
        $AvifApiStatus = 'STATIC HEADER API NAMES PRESENT; BINARY COMPATIBILITY NOT TESTED'
    }
    else {
        $AvifApiStatus = 'STATIC HEADER MISSING: ' + ($missingApiNames -join ', ')
    }
}

$AvifLibraryDetails = @(
    foreach ($libraryPath in $AvifLibraries) {
        $extension = [IO.Path]::GetExtension($libraryPath).ToLowerInvariant()
        $fileType = 'UNKNOWN'
        $linkage = 'UNKNOWN'
        $runtimeRequirement = 'UNKNOWN'
        if ($extension -eq '.dll') {
            $fileType = 'PE DLL'
            $linkage = 'DYNAMIC'
            $runtimeRequirement = 'YES'
        }
        elseif ($extension -eq '.lib') {
            $fileType = 'COFF library or import library'
            $linkage = 'STATIC OR IMPORT; NOT DISTINGUISHED'
            $runtimeRequirement = 'UNKNOWN UNTIL LINKAGE TYPE IS ESTABLISHED'
        }
        elseif ($extension -eq '.a') {
            $fileType = 'GNU archive'
            $linkage = 'STATIC OR IMPORT; NOT DISTINGUISHED'
            $runtimeRequirement = 'UNKNOWN UNTIL LINKAGE TYPE IS ESTABLISHED'
        }
        [ordered]@{
            Path = $libraryPath.Substring($Root.Length).TrimStart('\')
            FileType = $fileType
            Architecture = if ($extension -eq '.dll') { Get-PeArchitecture $libraryPath } else { 'UNKNOWN' }
            Version = Get-FileVersionMetadata $libraryPath
            Linkage = $linkage
            RuntimeDllRequirement = $runtimeRequirement
            Provenance = 'Approved repository dependency directory'
        }
    }
)

if ($AvifHeaders.Count -eq 0) {
    $AvifApprovedStatus = 'MISSING'
    $AvifReason = 'avif/avif.h is missing from approved repository dependency directories.'
}
elseif ($AvifLibraryDetails.Count -eq 0) {
    $AvifApprovedStatus = 'MISSING'
    $AvifReason = 'No libavif library candidate exists in approved repository dependency directories.'
}
elseif ($AvifApiStatus -notlike 'STATIC HEADER API NAMES PRESENT*') {
    $AvifApprovedStatus = 'UNKNOWN'
    $AvifReason = 'Required libavif API compatibility was not established from the discovered header.'
}
else {
    $AvifApprovedStatus = 'FOUND — APPROVAL REQUIRED'
    $AvifReason = 'libavif source, licensing and binary compatibility require explicit approval; compilation and linking were not performed.'
}
Add-DependencyRow 'libavif / avif.h' 'src/SharnouEngine.cpp' $(if ($AvifHeaders.Count -gt 0) { 'FOUND' } else { 'MISSING' }) $(if ($AvifLibraryDetails.Count -gt 0) { 'FOUND' } else { 'MISSING' }) $AvifVersion 'UNKNOWN' ((@($AvifHeaders | ForEach-Object { $_.Substring($Root.Length).TrimStart('\') }) + @($AvifLibraryDetails | ForEach-Object { $_.Path })) -join '; ') 'Approved repository dependency directories only' $AvifApprovedStatus $AvifReason 'NOT TESTED'
Add-BlockingReason "libavif: $AvifReason"

$SourceFiles = @()
$srcRoot = Join-Path $Root 'src'
if (Test-Path -LiteralPath $srcRoot -PathType Container) {
    $SourceFiles = @(Get-ChildItem -LiteralPath $srcRoot -Recurse -File -Force -ErrorAction SilentlyContinue | Where-Object { $_.Extension -in @('.cpp', '.hpp', '.h') })
}
$RuntimeLiteralPatterns = @(
    'Build/Runtime/Generated',
    'Build\Runtime\Generated',
    'Build/Runtime',
    'Build\Runtime',
    'assets/',
    'assets\',
    'assets/3d/generated',
    'assets\3d\generated',
    'assets/textures',
    'assets\textures',
    '.gltf',
    '.glb',
    '.ktx2',
    '.avif'
)
foreach ($sourceFile in $SourceFiles) {
    $lineNumber = 0
    $lines = @(Get-Content -LiteralPath $sourceFile.FullName -Encoding UTF8 -ErrorAction SilentlyContinue)
    foreach ($line in $lines) {
        $lineNumber++
        foreach ($literal in $RuntimeLiteralPatterns) {
            if ($line.IndexOf($literal, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
                [void]$RuntimeReferences.Add([ordered]@{
                    File = $sourceFile.FullName.Substring($Root.Length).TrimStart('\')
                    Line = $lineNumber
                    MatchedLiteral = $literal
                    Source = $line.Trim()
                })
            }
        }
    }
}

$DirectPathReferences = @()
$engineText = ''
if (Test-Path -LiteralPath $EngineSource -PathType Leaf) {
    $engineText = Get-Content -LiteralPath $EngineSource -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
}
if ($engineText) {
    $matches = [regex]::Matches($engineText, '"([^"]*(?:/|\\)[^"]*(?:\.json|\.gltf|\.glb|\.ktx2|\.avif)[^"]*)"')
    foreach ($match in $matches) {
        $DirectPathReferences += [ordered]@{
            PathLiteral = $match.Groups[1].Value
        }
    }
}

$GeneratedRuntimeDirectory = Join-Path $Root 'Build\Runtime\Generated'
$GeneratedRuntimeFiles = @()
if (Test-Path -LiteralPath $GeneratedRuntimeDirectory -PathType Container) {
    $GeneratedRuntimeFiles = @(
        Get-ChildItem -LiteralPath $GeneratedRuntimeDirectory -Recurse -File -Force -ErrorAction SilentlyContinue |
        ForEach-Object { $_.FullName.Substring($Root.Length).TrimStart('\') }
    )
}
$ExpectedPlanPath = Join-Path $Root 'Build\Runtime\Generated\honour_war_runtime_asset_plan.json'
$ExpectedPlanStatus = if (Test-Path -LiteralPath $ExpectedPlanPath -PathType Leaf) { 'FOUND' } else { 'MISSING' }

if ($BlockingReasons.Count -eq 0) {
    $Outcome = 'READY FOR APPROVED BUILD DESIGN'
}
else {
    $Outcome = 'BLOCKED — DEPENDENCY DECISION REQUIRED'
}

$TimestampUtc = [DateTime]::UtcNow.ToString('o')
$Report = [ordered]@{
    Schema = 'sharnou-engine-dependency-preflight/v1'
    preflight_execution_status = 'COMPLETED'
    Mode = 'READ_ONLY'
    Repository = $Repository
    ExpectedCommit = $ExpectedCommit
    CheckedOutCommit = $commit
    WorkflowRunId = $env:GITHUB_RUN_ID
    WorkflowRunAttempt = $env:GITHUB_RUN_ATTEMPT
    TimestampUtc = $TimestampUtc
    Runner = [ordered]@{
        Image = $runnerImage
        ImageVersion = $runnerImageVersion
        WindowsCaption = $os.Caption
        WindowsVersion = $os.Version
        WindowsBuild = $os.BuildNumber
        ProcessorArchitecture = $runnerArchitecture
        ComputerSystemType = $computer.SystemType
    }
    CompilerDiscovery = [ordered]@{
        Discovered = ($clMetadata.Count -gt 0)
        Candidates = $clMetadata
        VersionMetadata = if ($clMetadata.Count -gt 0) { 'KNOWN OR UNKNOWN PER CANDIDATE' } else { 'UNKNOWN' }
        Functionality = 'NOT TESTED'
    }
    LinkerDiscovery = [ordered]@{
        Discovered = ($linkMetadata.Count -gt 0)
        Candidates = $linkMetadata
        VersionMetadata = if ($linkMetadata.Count -gt 0) { 'KNOWN OR UNKNOWN PER CANDIDATE' } else { 'UNKNOWN' }
        Functionality = 'NOT TESTED'
    }
    VisualStudioInstances = $VisualStudioInstances.ToArray()
    MsvcToolsets = $MsvcToolsets.ToArray()
    WindowsSdk = [ordered]@{
        KitsRoot10 = $kitsRoot
        Versions = $SdkVersions.ToArray()
        IncludeRoots = $SdkIncludeRoots
        LibraryRoots = $SdkLibraryRoots
    }
    IncludeScan = [ordered]@{
        Label = 'STATIC REPOSITORY-LOCAL INCLUDE SCAN'
        WindowsSdkHeaders = 'DISCOVERED SEPARATELY'
        ExternalHeaders = 'DISCOVERED SEPARATELY'
        ConditionalCompilerIncludeResolution = 'NOT TESTED'
        LocalIncludes = $LocalIncludes.ToArray()
        ExternalIncludes = $ExternalIncludes.ToArray()
        ReachableLocalFiles = @($reachableFiles | ForEach-Object { $_.Substring($Root.Length).TrimStart('\') })
    }
    Libavif = [ordered]@{
        HeaderStatus = if ($AvifHeaders.Count -gt 0) { 'FOUND' } else { 'MISSING' }
        HeaderCandidates = @($AvifHeaders | ForEach-Object { $_.Substring($Root.Length).TrimStart('\') })
        LibraryStatus = if ($AvifLibraryDetails.Count -gt 0) { 'FOUND' } else { 'MISSING' }
        Libraries = $AvifLibraryDetails
        StaticLinkagePossible = @($AvifLibraryDetails | Where-Object { $_.Linkage -like 'STATIC*' }).Count -gt 0
        DynamicLinkagePossible = @($AvifLibraryDetails | Where-Object { $_.Linkage -eq 'DYNAMIC' }).Count -gt 0
        Version = $AvifVersion
        Architecture = if ($AvifLibraryDetails.Count -eq 0) { 'UNKNOWN' } else { (($AvifLibraryDetails | ForEach-Object Architecture) -join '; ') }
        SourceProvenance = 'Approved repository dependency directories only'
        LicenseEvidence = @($LicenseEvidence | ForEach-Object { $_.Substring($Root.Length).TrimStart('\') })
        RuntimeDllRequirement = if (@($AvifLibraryDetails | Where-Object { $_.RuntimeDllRequirement -eq 'YES' }).Count -gt 0) { 'YES' } elseif ($AvifLibraryDetails.Count -gt 0) { 'NOT ESTABLISHED' } else { 'NOT ESTABLISHED' }
        ApiCompatibility = $AvifApiStatus
    }
    RuntimeAssumptions = $RuntimeReferences.ToArray()
    DirectEnginePathReferences = @($DirectPathReferences)
    GeneratedRuntimeDirectory = [ordered]@{
        Path = 'Build/Runtime/Generated'
        ExistingFiles = $GeneratedRuntimeFiles
        ExpectedPlan = 'Build/Runtime/Generated/honour_war_runtime_asset_plan.json'
        ExpectedPlanStatus = $ExpectedPlanStatus
    }
    Dependencies = $Rows.ToArray()
    BlockingReasons = @($BlockingReasons | Sort-Object -Unique)
    CompilerFunctionality = 'NOT TESTED'
    LinkerFunctionality = 'NOT TESTED'
    Compilation = 'NOT ATTEMPTED'
    Linking = 'NOT ATTEMPTED'
    Installation = 'NOT PERFORMED'
    Download = 'NOT PERFORMED'
    DependencyRestoration = 'NOT PERFORMED'
    PackageManager = 'NOT INVOKED'
    Packaging = 'NOT PERFORMED'
    ExecutableGeneration = 'NOT ATTEMPTED'
    Release = 'NOT PERFORMED'
    FakeExecutableCreation = 'NOT PERFORMED'
    Outcome = $Outcome
}

$Report | ConvertTo-Json -Depth 40 | Set-Content -LiteralPath $JsonFile -Encoding UTF8

$MarkdownLines = [System.Collections.Generic.List[string]]::new()
[void]$MarkdownLines.Add('# Sharnou Engine Dependency Preflight')
[void]$MarkdownLines.Add('')
[void]$MarkdownLines.Add("Repository: $Repository")
[void]$MarkdownLines.Add("Expected commit: $ExpectedCommit")
[void]$MarkdownLines.Add("Checked-out commit: $commit")
[void]$MarkdownLines.Add("Runner image: $runnerImage")
[void]$MarkdownLines.Add("Runner image version: $runnerImageVersion")
[void]$MarkdownLines.Add("Windows version: $($os.Version)")
[void]$MarkdownLines.Add("Windows build: $($os.BuildNumber)")
[void]$MarkdownLines.Add("Architecture: $runnerArchitecture")
[void]$MarkdownLines.Add("Workflow run ID: $($env:GITHUB_RUN_ID)")
[void]$MarkdownLines.Add("Timestamp UTC: $TimestampUtc")
[void]$MarkdownLines.Add('')
[void]$MarkdownLines.Add("cl.exe discovered: $($clMetadata.Count -gt 0)")
[void]$MarkdownLines.Add('Compiler version metadata: KNOWN OR UNKNOWN PER CANDIDATE')
[void]$MarkdownLines.Add('Compiler functionality: NOT TESTED')
[void]$MarkdownLines.Add("link.exe discovered: $($linkMetadata.Count -gt 0)")
[void]$MarkdownLines.Add('Linker version metadata: KNOWN OR UNKNOWN PER CANDIDATE')
[void]$MarkdownLines.Add('Linker functionality: NOT TESTED')
[void]$MarkdownLines.Add('')
[void]$MarkdownLines.Add('Compilation: NOT ATTEMPTED')
[void]$MarkdownLines.Add('Linking: NOT ATTEMPTED')
[void]$MarkdownLines.Add('Installation: NOT PERFORMED')
[void]$MarkdownLines.Add('Dependency download: NOT PERFORMED')
[void]$MarkdownLines.Add('Dependency restoration: NOT PERFORMED')
[void]$MarkdownLines.Add('Package manager: NOT INVOKED')
[void]$MarkdownLines.Add('Packaging: NOT PERFORMED')
[void]$MarkdownLines.Add('Executable generation: NOT ATTEMPTED')
[void]$MarkdownLines.Add('Release: NOT PERFORMED')
[void]$MarkdownLines.Add('')
[void]$MarkdownLines.Add('## Dependency status')
[void]$MarkdownLines.Add('')
[void]$MarkdownLines.Add('| Dependency | Required source file | Header status | Library status | Version | Architecture | Path | Provenance | Approved status | Blocking reason |')
[void]$MarkdownLines.Add('|---|---|---|---|---|---|---|---|---|---|')
foreach ($row in $Rows) {
    $cellValues = @(
        $row.Dependency,
        $row.'Required source file',
        $row.'Header status',
        $row.'Library status',
        $row.Version,
        $row.Architecture,
        $row.Path,
        $row.Provenance,
        $row.'Approved status',
        $row.'Blocking reason'
    )
    $escapedCells = @($cellValues | ForEach-Object { ([string]$_) -replace '\|', '\|' })
    [void]$MarkdownLines.Add('| ' + ($escapedCells -join ' | ') + ' |')
}
[void]$MarkdownLines.Add('')
[void]$MarkdownLines.Add('## libavif')
[void]$MarkdownLines.Add("Header status: $(if ($AvifHeaders.Count -gt 0) { 'FOUND' } else { 'MISSING' })")
[void]$MarkdownLines.Add("Library status: $(if ($AvifLibraryDetails.Count -gt 0) { 'FOUND' } else { 'MISSING' })")
[void]$MarkdownLines.Add("Version: $AvifVersion")
[void]$MarkdownLines.Add("Static linkage possible: $(@($AvifLibraryDetails | Where-Object { $_.Linkage -like 'STATIC*' }).Count -gt 0)")
[void]$MarkdownLines.Add("Dynamic linkage possible: $(@($AvifLibraryDetails | Where-Object { $_.Linkage -eq 'DYNAMIC' }).Count -gt 0)")
[void]$MarkdownLines.Add("Runtime DLL implication: $($Report.Libavif.RuntimeDllRequirement)")
[void]$MarkdownLines.Add('Source/provenance: approved repository dependency directories only')
[void]$MarkdownLines.Add("License evidence: " + ($LicenseEvidence -join '; '))
[void]$MarkdownLines.Add("API compatibility: $AvifApiStatus")
[void]$MarkdownLines.Add('')
[void]$MarkdownLines.Add('## Include scan')
[void]$MarkdownLines.Add('STATIC REPOSITORY-LOCAL INCLUDE SCAN')
[void]$MarkdownLines.Add('Windows SDK headers discovered separately.')
[void]$MarkdownLines.Add('External headers discovered separately.')
[void]$MarkdownLines.Add('Conditional compiler include resolution NOT TESTED.')
[void]$MarkdownLines.Add('')
[void]$MarkdownLines.Add('## Blocking reasons')
[void]$MarkdownLines.Add('')
if ($BlockingReasons.Count -eq 0) {
    [void]$MarkdownLines.Add('NONE')
}
else {
    foreach ($reason in @($BlockingReasons | Sort-Object -Unique)) {
        [void]$MarkdownLines.Add('- ' + $reason)
    }
}
[void]$MarkdownLines.Add('')
[void]$MarkdownLines.Add('## Final outcome')
[void]$MarkdownLines.Add('')
[void]$MarkdownLines.Add($Outcome)
$MarkdownLines | Set-Content -LiteralPath $MarkdownFile -Encoding UTF8

$IncludeLines = [System.Collections.Generic.List[string]]::new()
[void]$IncludeLines.Add('STATIC REPOSITORY-LOCAL INCLUDE SCAN')
[void]$IncludeLines.Add('Windows SDK headers are discovered separately.')
[void]$IncludeLines.Add('External headers are discovered separately.')
[void]$IncludeLines.Add('Conditional compiler include resolution NOT TESTED.')
[void]$IncludeLines.Add('')
[void]$IncludeLines.Add('Repository-local includes:')
if ($LocalIncludes.Count -eq 0) {
    [void]$IncludeLines.Add('NONE FOUND')
}
else {
    foreach ($includeRecord in $LocalIncludes) {
        [void]$IncludeLines.Add("$($includeRecord.File):$($includeRecord.Line): $($includeRecord.Include) -> $($includeRecord.Resolved) [$($includeRecord.Status)]")
    }
}
[void]$IncludeLines.Add('')
[void]$IncludeLines.Add('External headers:')
if ($ExternalIncludes.Count -eq 0) {
    [void]$IncludeLines.Add('NONE FOUND')
}
else {
    foreach ($includeRecord in $ExternalIncludes) {
        [void]$IncludeLines.Add("$($includeRecord.File):$($includeRecord.Line): <$($includeRecord.Include)>")
        [void]$IncludeLines.Add("  Resolution: $($includeRecord.Resolution)")
    }
}
$IncludeLines | Set-Content -LiteralPath $IncludeFile -Encoding UTF8

$RuntimeLines = [System.Collections.Generic.List[string]]::new()
[void]$RuntimeLines.Add('RUNTIME ASSUMPTIONS REPORT')
[void]$RuntimeLines.Add('Static source scan only; runtime execution was NOT PERFORMED.')
[void]$RuntimeLines.Add('Literal substring matching is used for configured runtime patterns.')
[void]$RuntimeLines.Add('')
[void]$RuntimeLines.Add('Direct path-like references from SharnouEngine.cpp:')
if ($DirectPathReferences.Count -eq 0) {
    [void]$RuntimeLines.Add('NONE FOUND')
}
else {
    foreach ($pathRecord in $DirectPathReferences) {
        [void]$RuntimeLines.Add($pathRecord.PathLiteral)
    }
}
[void]$RuntimeLines.Add('')
[void]$RuntimeLines.Add('Configured runtime literal matches:')
if ($RuntimeReferences.Count -eq 0) {
    [void]$RuntimeLines.Add('NONE FOUND')
}
else {
    foreach ($runtimeRecord in $RuntimeReferences) {
        [void]$RuntimeLines.Add("$($runtimeRecord.File):$($runtimeRecord.Line): [$($runtimeRecord.MatchedLiteral)] $($runtimeRecord.Source)")
    }
}
[void]$RuntimeLines.Add('')
[void]$RuntimeLines.Add('Build/Runtime/Generated existing files:')
if ($GeneratedRuntimeFiles.Count -eq 0) {
    [void]$RuntimeLines.Add('NONE FOUND')
}
else {
    foreach ($generatedFile in $GeneratedRuntimeFiles) {
        [void]$RuntimeLines.Add($generatedFile)
    }
}
[void]$RuntimeLines.Add('')
[void]$RuntimeLines.Add('Expected generated runtime plan:')
if ($ExpectedPlanStatus -eq 'FOUND') {
    [void]$RuntimeLines.Add('FOUND: Build/Runtime/Generated/honour_war_runtime_asset_plan.json')
}
else {
    [void]$RuntimeLines.Add('MISSING: Build/Runtime/Generated/honour_war_runtime_asset_plan.json')
}
$RuntimeLines | Set-Content -LiteralPath $RuntimeFile -Encoding UTF8

Write-Output $Outcome
return
