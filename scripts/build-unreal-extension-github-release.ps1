param(
    [Parameter(Mandatory = $true)]
    [string]$EngineRoot,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [Parameter(Mandatory = $true)]
    [ValidatePattern("^[0-9a-fA-F]{40}$")]
    [string]$SourceCommit,

    [string]$EngineVersionLabel,

    [ValidatePattern("^[A-Za-z0-9]+(\+[A-Za-z0-9]+)*$")]
    [string]$TargetPlatforms = "Win64"
)

$ErrorActionPreference = "Stop"

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$pluginDescriptor = Join-Path $repositoryRoot "UnrealToolsetsExtension.uplugin"
$packageScript = Join-Path $PSScriptRoot "package-unreal-extension.ps1"
$resolvedOutput = [IO.Path]::GetFullPath($OutputDirectory)
$stagingRoot = "$resolvedOutput.staging"
$packageDirectory = Join-Path $stagingRoot "UnrealToolsetsExtension"

if (-not (Test-Path -LiteralPath $pluginDescriptor -PathType Leaf)) {
    throw "Plugin descriptor not found: $pluginDescriptor"
}

if (-not (Test-Path -LiteralPath $packageScript -PathType Leaf)) {
    throw "Packaging script not found: $packageScript"
}

if (Test-Path -LiteralPath $resolvedOutput) {
    throw "OutputDirectory already exists. Choose a new path: $resolvedOutput"
}

if (Test-Path -LiteralPath $stagingRoot) {
    throw "Staging directory already exists. Remove it after checking its contents: $stagingRoot"
}

$actualCommit = (& git -C $repositoryRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($actualCommit)) {
    throw "Unable to resolve the repository HEAD commit."
}

if ($actualCommit -ne $SourceCommit) {
    throw "SourceCommit does not match repository HEAD. Expected $actualCommit."
}

$workingTreeStatus = & git -C $repositoryRoot status --porcelain --untracked-files=normal
if ($LASTEXITCODE -ne 0) {
    throw "Unable to inspect the repository working tree."
}

if ($workingTreeStatus) {
    throw "Refusing to build GitHub Release assets from a dirty working tree."
}

$descriptor = Get-Content -Raw -LiteralPath $pluginDescriptor | ConvertFrom-Json
$pluginVersion = [string]$descriptor.VersionName
if ([string]::IsNullOrWhiteSpace($pluginVersion)) {
    throw "The plugin descriptor has no VersionName."
}

$buildVersionPath = Join-Path $EngineRoot "Engine\Build\Build.version"
$buildVersion = Get-Content -Raw -LiteralPath $buildVersionPath | ConvertFrom-Json
foreach ($field in @("MajorVersion", "MinorVersion", "PatchVersion", "Changelist")) {
    $value = $buildVersion.$field
    if (($value -isnot [int] -and $value -isnot [long]) -or $value -lt 0) {
        throw "Invalid $field in Engine/Build/Build.version."
    }
}
$engineLabel = "UE{0}.{1}.{2}" -f $buildVersion.MajorVersion, $buildVersion.MinorVersion, $buildVersion.PatchVersion
if ($PSBoundParameters.ContainsKey("EngineVersionLabel") -and $EngineVersionLabel -cne $engineLabel) {
    throw "EngineVersionLabel must match the installed build ($engineLabel)."
}
$platforms = @($TargetPlatforms -split "\+")
$assetBaseName = "UnrealToolsetsExtension-$pluginVersion-$engineLabel-CL$($buildVersion.Changelist)-$TargetPlatforms"
$assetFileName = "$assetBaseName.zip"
$assetPath = Join-Path $resolvedOutput $assetFileName
$checksumFileName = "$assetFileName.sha256"
$checksumPath = Join-Path $resolvedOutput $checksumFileName
$manifestPath = Join-Path $resolvedOutput "$assetBaseName.manifest.json"
$utf8NoBom = [Text.UTF8Encoding]::new($false)

$createdStage = $false
$createdOutput = $false
try {
    New-Item -ItemType Directory -Path $stagingRoot | Out-Null
    $createdStage = $true

    & $packageScript `
        -EngineRoot $EngineRoot `
        -OutputDirectory $packageDirectory `
        -TargetPlatforms $TargetPlatforms

    if ($LASTEXITCODE -ne 0) {
        throw "Unreal extension packaging failed with exit code $LASTEXITCODE."
    }

    foreach ($platform in $platforms) {
        $moduleFile = Join-Path $packageDirectory "Binaries\$platform\UnrealEditor.modules"
        $modules = Get-Content -Raw -LiteralPath $moduleFile | ConvertFrom-Json
        $binary = [string]$modules.Modules.UnrealToolsetsExtension
        if (-not $binary -or $binary -ne [IO.Path]::GetFileName($binary) -or
            -not (Test-Path -LiteralPath (Join-Path (Split-Path $moduleFile) $binary) -PathType Leaf)) {
            throw "Missing UnrealToolsetsExtension Editor binary for $platform."
        }
    }
    New-Item -ItemType Directory -Path $resolvedOutput | Out-Null
    $createdOutput = $true
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [IO.Compression.ZipFile]::Open($assetPath, "Create")
    try {
        foreach ($file in Get-ChildItem -LiteralPath $packageDirectory -Recurse -File) {
            $entryName = $file.FullName.Substring($stagingRoot.Length + 1).Replace("\", "/")
            [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
                $archive, $file.FullName, $entryName, [IO.Compression.CompressionLevel]::Optimal)
        }
    }
    finally {
        $archive.Dispose()
    }

    $sha256 = [Security.Cryptography.SHA256]::Create()
    $stream = [IO.File]::OpenRead($assetPath)
    try {
        $digest = [BitConverter]::ToString($sha256.ComputeHash($stream)).Replace("-", "").ToLowerInvariant()
    }
    finally {
        $stream.Dispose()
        $sha256.Dispose()
    }
    $checksumLine = "$digest  $assetFileName`n"
    [IO.File]::WriteAllText($checksumPath, $checksumLine, $utf8NoBom)

    $manifest = [ordered]@{
        schemaVersion = 1
        pluginName = "UnrealToolsetsExtension"
        pluginVersion = $pluginVersion
        sourceCommit = $SourceCommit.ToLowerInvariant()
        engineVersion = $engineLabel
        engineChangelist = $buildVersion.Changelist
        targetPlatforms = $platforms
        assets = @(
            [ordered]@{
                file = $assetFileName
                sha256 = $digest
            }
        )
    }
    $manifestJson = $manifest | ConvertTo-Json -Depth 5
    [IO.File]::WriteAllText($manifestPath, "$manifestJson`n", $utf8NoBom)
}
catch {
    if ($createdOutput -and (Test-Path -LiteralPath $resolvedOutput -PathType Container)) {
        $outputParent = [IO.Path]::GetDirectoryName($resolvedOutput)
        $resolvedParent = [IO.Path]::GetFullPath($outputParent)
        if (-not $resolvedOutput.StartsWith(
            "$resolvedParent$([IO.Path]::DirectorySeparatorChar)",
            [StringComparison]::OrdinalIgnoreCase
        )) {
            throw "Refusing to clean an unexpected release output path: $resolvedOutput"
        }
        Remove-Item -LiteralPath $resolvedOutput -Recurse -Force
    }
    throw
}
finally {
    if ($createdStage -and (Test-Path -LiteralPath $stagingRoot -PathType Container)) {
        $expectedStaging = "$resolvedOutput.staging"
        $resolvedStaging = (Resolve-Path -LiteralPath $stagingRoot).Path
        if ($resolvedStaging -ne $expectedStaging) {
            throw "Refusing to clean an unexpected staging path: $resolvedStaging"
        }
        Remove-Item -LiteralPath $resolvedStaging -Recurse -Force
    }
}

Write-Output "GitHub Release assets prepared: $resolvedOutput"
Write-Output "Asset: $assetFileName"
Write-Output "SHA256: $digest"
