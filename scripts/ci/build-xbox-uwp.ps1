[CmdletBinding()]
param(
    [string]$RepositoryRoot,
    [string]$ProjectPath = "ui\xui\uwp\xemu-uwp.vcxproj",
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [string]$SdkVersion = "10.0.26100.0",
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory
)

$ErrorActionPreference = "Stop"

function Fail([string]$Message) {
    throw "xemu Xbox UWP build failed: $Message"
}

function Get-RelativePath([string]$BasePath, [string]$TargetPath) {
    $baseUri = New-Object Uri(([IO.Path]::GetFullPath($BasePath).TrimEnd('\') + '\'))
    $targetUri = New-Object Uri([IO.Path]::GetFullPath($TargetPath))
    return [Uri]::UnescapeDataString($baseUri.MakeRelativeUri($targetUri).ToString()).Replace('/', '\')
}

if (-not $RepositoryRoot) {
    $RepositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}
$RepositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot -ErrorAction Stop).Path
if ($Platform -ne "x64") { Fail "only x64 is supported by Xbox Series X|S Dev Mode." }

$validator = Join-Path $PSScriptRoot "validate-xbox-uwp.ps1"
& $validator -RepositoryRoot $RepositoryRoot -ProjectPath $ProjectPath | Write-Output
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$projectFullPath = if ([IO.Path]::IsPathRooted($ProjectPath)) {
    [IO.Path]::GetFullPath($ProjectPath)
} else {
    [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $ProjectPath))
}
$projectRoot = Split-Path -Parent $projectFullPath
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$repositoryPrefix = $RepositoryRoot.TrimEnd('\') + '\'
if ($OutputDirectory -eq [IO.Path]::GetPathRoot($OutputDirectory) -or
    -not $OutputDirectory.StartsWith($repositoryPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    Fail "OutputDirectory must be a dedicated directory below the repository root."
}
if (Test-Path -LiteralPath $OutputDirectory) {
    Remove-Item -LiteralPath $OutputDirectory -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$unsignedOutput = Join-Path $OutputDirectory "unsigned"
$msbuildOutput = Join-Path $unsignedOutput "msbuild-package"
New-Item -ItemType Directory -Force -Path $msbuildOutput | Out-Null

$vswhereCandidates = @(
    (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"),
    (Join-Path ${env:ProgramFiles} "Microsoft Visual Studio\Installer\vswhere.exe")
) | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) }
$msbuild = $null
foreach ($vswhere in $vswhereCandidates) {
    $found = & $vswhere -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -latest -find MSBuild\**\Bin\MSBuild.exe 2>$null |
        Select-Object -First 1
    if ($found) { $msbuild = [string]$found; break }
}
if (-not $msbuild) {
    $msbuildCommand = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($msbuildCommand) { $msbuild = $msbuildCommand.Source }
}
if (-not $msbuild -or -not (Test-Path -LiteralPath $msbuild -PathType Leaf)) {
    Fail "MSBuild with the Visual C++ tools was not found."
}

$sdkBin = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin\$SdkVersion\x64"
$makeAppx = Join-Path $sdkBin "makeappx.exe"
$signtool = Join-Path $sdkBin "signtool.exe"
foreach ($tool in @($makeAppx, $signtool)) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) {
        Fail "Windows SDK $SdkVersion tool is missing: $tool"
    }
}
$sdkInclude = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\Include\$SdkVersion\um\windows.h"
if (-not (Test-Path -LiteralPath $sdkInclude -PathType Leaf)) {
    Fail "Windows SDK $SdkVersion headers are missing: $sdkInclude"
}

$appxDir = $msbuildOutput.TrimEnd('\') + "\"
$msbuildArgs = @(
    $projectFullPath,
    "/m",
    "/t:Rebuild",
    "/p:Configuration=$Configuration",
    "/p:Platform=$Platform",
    "/p:WindowsTargetPlatformVersion=$SdkVersion",
    "/p:TargetPlatformVersion=$SdkVersion",
    "/p:AppxBundle=Never",
    "/p:AppxPackageSigningEnabled=false",
    "/p:GenerateAppxPackageOnBuild=true",
    "/p:AppxPackageDir=$appxDir"
)
Write-Host "Building full xemu UWP target with $msbuild"
& $msbuild @msbuildArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$packages = @(Get-ChildItem -LiteralPath $msbuildOutput -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Extension.ToLowerInvariant() -in @('.msix', '.appx') })
if ($packages.Count -ne 1) {
    Fail "expected exactly one unsigned x64 MSIX/AppX from the full target; found $($packages.Count) below '$msbuildOutput'."
}
$package = $packages[0]
$packageOut = Join-Path $unsignedOutput ("xemu-xbox-$Configuration-x64" + $package.Extension.ToLowerInvariant())
Copy-Item -LiteralPath $package.FullName -Destination $packageOut -Force

& $makeAppx validate /p $packageOut
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$symbolsOut = Join-Path $OutputDirectory "symbols"
New-Item -ItemType Directory -Force -Path $symbolsOut | Out-Null
$symbols = @(Get-ChildItem -LiteralPath $projectRoot -Recurse -File -Filter *.pdb -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -notmatch '(?i)\\obj\\' })
foreach ($symbol in $symbols) {
    Copy-Item -LiteralPath $symbol.FullName -Destination (Join-Path $symbolsOut $symbol.Name) -Force
}
if ($symbols.Count -eq 0) {
    Fail "the full UWP build produced no PDB symbols; a release is not allowed without symbols."
}

[pscustomobject]@{
    Configuration = $Configuration
    Platform = $Platform
    SdkVersion = $SdkVersion
    Project = (Get-RelativePath $RepositoryRoot $projectFullPath).Replace('\', '/')
    Package = (Get-RelativePath $OutputDirectory $packageOut).Replace('\', '/')
    SymbolCount = $symbols.Count
    Msbuild = $msbuild
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputDirectory "build-metadata.json") -Encoding UTF8

Write-Host "Built unsigned package: $packageOut"
