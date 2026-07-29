param(
    [Parameter(Mandatory = $true)]
    [string]$EngineRoot,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [string]$TargetPlatforms = "Win64"
)

$ErrorActionPreference = "Stop"

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$pluginPath = Join-Path $repositoryRoot "CommunityToolsetsExtension.uplugin"
$runUat = Join-Path $EngineRoot "Engine\Build\BatchFiles\RunUAT.bat"
$resolvedOutput = [IO.Path]::GetFullPath($OutputDirectory)

if (-not (Test-Path -LiteralPath $pluginPath -PathType Leaf)) {
    throw "Plugin descriptor not found: $pluginPath"
}

if (-not (Test-Path -LiteralPath $runUat -PathType Leaf)) {
    throw "RunUAT.bat not found under EngineRoot: $runUat"
}

if (Test-Path -LiteralPath $resolvedOutput) {
    throw "OutputDirectory already exists. Choose a new empty path: $resolvedOutput"
}

& $runUat BuildPlugin `
    "-Plugin=$pluginPath" `
    "-Package=$resolvedOutput" `
    "-TargetPlatforms=$TargetPlatforms" `
    -Rocket

if ($LASTEXITCODE -ne 0) {
    throw "Unreal BuildPlugin failed with exit code $LASTEXITCODE."
}

$intermediateDirectory = [IO.Path]::GetFullPath(
    (Join-Path $resolvedOutput "Intermediate")
)
$outputPrefix = $resolvedOutput.TrimEnd(
    [IO.Path]::DirectorySeparatorChar,
    [IO.Path]::AltDirectorySeparatorChar
) + [IO.Path]::DirectorySeparatorChar

if (-not $intermediateDirectory.StartsWith(
    $outputPrefix,
    [StringComparison]::OrdinalIgnoreCase
)) {
    throw "Refusing to clean an Intermediate directory outside the package: $intermediateDirectory"
}

if (Test-Path -LiteralPath $intermediateDirectory -PathType Container) {
    Remove-Item -LiteralPath $intermediateDirectory -Recurse -Force
}

Write-Output "Packaged CommunityToolsetsExtension: $resolvedOutput"
