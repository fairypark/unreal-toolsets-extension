param(
    [Parameter(Mandatory = $true)]
    [string]$EngineRoot,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [Parameter(Mandatory = $true)]
    [ValidatePattern("^[0-9a-fA-F]{40}$")]
    [string]$SourceCommit,

    [string]$EngineVersionLabel = "UE5.8",

    [string]$TargetPlatforms = "Win64"
)

$ErrorActionPreference = "Stop"

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$pluginDescriptor = Join-Path $repositoryRoot "CommunityToolsetsExtension.uplugin"
$packageScript = Join-Path $PSScriptRoot "package-unreal-extension.ps1"
$resolvedOutput = [IO.Path]::GetFullPath($OutputDirectory)
$stagingRoot = "$resolvedOutput.staging"
$packageDirectory = Join-Path $stagingRoot "CommunityToolsetsExtension"

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

$safeEngineLabel = $EngineVersionLabel -replace "[^A-Za-z0-9._-]", "-"
$safePlatforms = $TargetPlatforms -replace "[^A-Za-z0-9._+-]", "-"
$assetBaseName = "CommunityToolsetsExtension-$pluginVersion-$safeEngineLabel-$safePlatforms"
$assetFileName = "$assetBaseName.zip"
$assetPath = Join-Path $resolvedOutput $assetFileName
$checksumFileName = "$assetFileName.sha256"
$checksumPath = Join-Path $resolvedOutput $checksumFileName
$manifestPath = Join-Path $resolvedOutput "release-manifest.json"
$utf8NoBom = [Text.UTF8Encoding]::new($false)

try {
    New-Item -ItemType Directory -Path $stagingRoot | Out-Null

    & $packageScript `
        -EngineRoot $EngineRoot `
        -OutputDirectory $packageDirectory `
        -TargetPlatforms $TargetPlatforms

    if ($LASTEXITCODE -ne 0) {
        throw "Unreal extension packaging failed with exit code $LASTEXITCODE."
    }

    New-Item -ItemType Directory -Path $resolvedOutput | Out-Null
    Compress-Archive `
        -LiteralPath $packageDirectory `
        -DestinationPath $assetPath `
        -CompressionLevel Optimal

    $hash = Get-FileHash -LiteralPath $assetPath -Algorithm SHA256
    $checksumLine = "$($hash.Hash.ToLowerInvariant())  $assetFileName`n"
    [IO.File]::WriteAllText($checksumPath, $checksumLine, $utf8NoBom)

    $manifest = [ordered]@{
        schemaVersion = 1
        pluginName = "CommunityToolsetsExtension"
        pluginVersion = $pluginVersion
        sourceCommit = $SourceCommit.ToLowerInvariant()
        engineVersion = $EngineVersionLabel
        targetPlatforms = @($TargetPlatforms -split "\+" | Where-Object { $_ })
        assets = @(
            [ordered]@{
                file = $assetFileName
                sha256 = $hash.Hash.ToLowerInvariant()
            }
        )
    }
    $manifestJson = $manifest | ConvertTo-Json -Depth 5
    [IO.File]::WriteAllText($manifestPath, "$manifestJson`n", $utf8NoBom)
}
catch {
    if (Test-Path -LiteralPath $resolvedOutput -PathType Container) {
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
    if (Test-Path -LiteralPath $stagingRoot -PathType Container) {
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
Write-Output "SHA256: $($hash.Hash)"
