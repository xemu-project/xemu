[CmdletBinding()]
param(
    [string]$RepositoryRoot,
    [string]$ProjectPath = "ui\xui\uwp\xemu-uwp.vcxproj",
    [string]$VclibsPackagePath,
    [string]$SdkVersion = "10.0.26100.0"
)

$ErrorActionPreference = "Stop"

function Fail([string]$Message) {
    throw "xemu Xbox UWP target gate failed: $Message"
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
$ProjectPath = if ([IO.Path]::IsPathRooted($ProjectPath)) {
    $ProjectPath
} else {
    Join-Path $RepositoryRoot $ProjectPath
}
$ProjectPath = [IO.Path]::GetFullPath($ProjectPath)

if (-not (Test-Path -LiteralPath $ProjectPath -PathType Leaf)) {
    Fail "the full xemu UWP project is not present at '$ProjectPath'. The checked-in uwp-host-probe is only a capability probe and is intentionally not a release target. Expected contract: ui\xui\uwp\xemu-uwp.vcxproj."
}
$projectRoot = Split-Path -Parent $ProjectPath
$projectRelative = Get-RelativePath $RepositoryRoot $ProjectPath
if ($projectRelative -match '(?i)uwp-host-probe') {
    Fail "the target project resolves to the host probe; a full xemu runtime project is required."
}

try {
    [xml]$project = Get-Content -LiteralPath $ProjectPath -Raw
} catch {
    Fail "the target project is not valid XML: $($_.Exception.Message)"
}
$projectText = Get-Content -LiteralPath $ProjectPath -Raw
if ($projectText -notmatch '(?i)<XemuUwpTarget>\s*true\s*</XemuUwpTarget>') {
    Fail "the project is missing the explicit <XemuUwpTarget>true</XemuUwpTarget> readiness marker. This prevents the probe from being mistaken for the emulator."
}
if ($projectText -notmatch '(?i)AppxManifest') {
    Fail "the target project does not include an AppxManifest item."
}
if ($projectText -notmatch '(?i)(PlatformToolset|WindowsTargetPlatformVersion|TargetPlatformVersion)') {
    Fail "the target project does not declare a Windows/UWP toolchain configuration."
}
if ($projectText -notmatch '(?i)(x64|Win32TargetPlatform)') {
    Fail "the target project has no x64 configuration. Xbox Series X|S Dev Mode requires the x64 package."
}

$manifestCandidates = @(
    Get-ChildItem -LiteralPath $projectRoot -Filter "Package.appxmanifest" -File -Recurse -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty FullName |
        Where-Object { $_ } |
        Select-Object -Unique
)
if ($manifestCandidates.Count -ne 1) {
    Fail "expected exactly one target Package.appxmanifest below '$projectRoot'; found $($manifestCandidates.Count)."
}
$manifestPath = [string]$manifestCandidates[0]
try {
    [xml]$manifest = Get-Content -LiteralPath $manifestPath -Raw
} catch {
    Fail "the target manifest is not valid XML: $($_.Exception.Message)"
}
$identity = $manifest.SelectSingleNode("/*[local-name()='Package']/*[local-name()='Identity']")
if (-not $identity) { Fail "target manifest has no Package/Identity." }
if ([string]$identity.ProcessorArchitecture -ne "x64") {
    Fail "target manifest Identity ProcessorArchitecture must be x64 (got '$($identity.ProcessorArchitecture)')."
}
if ([string]$identity.Name -eq "xemu.UwpHostProbe" -or [string]$identity.Publisher -eq "CN=xemu-development") {
    Fail "target manifest still uses the host-probe identity/publisher."
}
if ([string]$identity.Publisher -match '(?i)development|placeholder|example') {
    Fail "target manifest publisher '$($identity.Publisher)' is a development placeholder. Supply the stable publisher matching the signing certificate."
}
$families = @($manifest.SelectNodes("/*[local-name()='Package']/*[local-name()='Dependencies']/*[local-name()='TargetDeviceFamily']"))
$xboxFamily = @($families | Where-Object { [string]$_.Name -eq "Windows.Xbox" })
if ($xboxFamily.Count -ne 1) {
    Fail "target manifest must declare exactly one Windows.Xbox TargetDeviceFamily for Xbox Dev Mode. Windows.Universal is allowed only as an additional explicitly supported family."
}
foreach ($family in $families) {
    if ([string]$family.Name -notin @("Windows.Xbox", "Windows.Universal")) {
        Fail "unsupported TargetDeviceFamily '$($family.Name)'; only Windows.Xbox and optional Windows.Universal are allowed."
    }
    $minVersion = $null
    $maxVersion = $null
    try {
        $minVersion = [version][string]$family.MinVersion
        $maxVersion = [version][string]$family.MaxVersionTested
    } catch {
        Fail "TargetDeviceFamily '$($family.Name)' must declare parseable MinVersion and MaxVersionTested."
    }
    if ($minVersion -lt [version]"10.0.14393.0" -or $maxVersion -lt $minVersion) {
        Fail "TargetDeviceFamily '$($family.Name)' has invalid MinVersion/MaxVersionTested."
    }
}
$application = $manifest.SelectSingleNode("/*[local-name()='Package']/*[local-name()='Applications']/*[local-name()='Application']")
if (-not $application -or [string]::IsNullOrWhiteSpace([string]$application.Id)) {
    Fail "target manifest has no launchable Application entry."
}
if ([string]$application.Executable -match '\$targetnametoken\$' -or
    [string]$application.Executable -notmatch '(?i)\.exe$' -or
    [string]::IsNullOrWhiteSpace([string]$application.EntryPoint)) {
    Fail "target manifest must contain an explicit launchable Executable ending in .exe and a non-empty EntryPoint."
}
$dependencies = @($manifest.SelectNodes("/*[local-name()='Package']/*[local-name()='Dependencies']/*[local-name()='PackageDependency']"))
if (@($dependencies | Where-Object { [string]$_.Name -match '(?i)UWPDesktop' }).Count -ne 0) {
    Fail "Microsoft.VCLibs.140.00.UWPDesktop is forbidden for Xbox Dev Mode."
}
$vclibs = @($dependencies | Where-Object { [string]$_.Name -eq "Microsoft.VCLibs.140.00" })
if ($vclibs.Count -ne 1 -or
    [string]$vclibs[0].Publisher -ne "CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US" -or
    [string]$vclibs[0].ProcessorArchitecture -notin @("", "x64") -or
    [version][string]$vclibs[0].MinVersion -lt [version]"14.0.33519.0") {
    Fail "target manifest must declare exactly one x64-compatible Microsoft.VCLibs.140.00 dependency at version 14.0.33519.0 or newer."
}
if ($VclibsPackagePath) {
    $makeAppx = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin\$SdkVersion\x64\makeappx.exe"
    if (-not (Test-Path -LiteralPath $makeAppx -PathType Leaf)) { Fail "Windows SDK MakeAppx is required to validate the staged VCLibs package: $makeAppx" }
    $VclibsPackagePath = (Resolve-Path -LiteralPath $VclibsPackagePath -ErrorAction Stop).Path
    $vclibsUnpack = Join-Path ([IO.Path]::GetTempPath()) ("xemu-vclibs-validate-" + [guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $vclibsUnpack | Out-Null
    try {
        & $makeAppx unpack /p $VclibsPackagePath /d $vclibsUnpack /o | Out-Null
        if ($LASTEXITCODE -ne 0) { Fail "MakeAppx could not unpack the staged VCLibs package." }
        $vclibsManifest = Join-Path $vclibsUnpack "AppxManifest.xml"
        if (-not (Test-Path -LiteralPath $vclibsManifest -PathType Leaf)) { Fail "staged VCLibs package has no AppxManifest.xml." }
        [xml]$vclibsPackageManifest = Get-Content -LiteralPath $vclibsManifest -Raw
        $vclibsIdentity = $vclibsPackageManifest.SelectSingleNode("/*[local-name()='Package']/*[local-name()='Identity']")
        if (-not $vclibsIdentity -or [string]$vclibsIdentity.Name -ne "Microsoft.VCLibs.140.00" -or [string]$vclibsIdentity.ProcessorArchitecture -ne "x64" -or [string]$vclibsIdentity.Publisher -ne "CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US") {
            Fail "staged VCLibs package identity is not the official x64 Microsoft.VCLibs.140.00 package."
        }
        $packageVersion = [version][string]$vclibsIdentity.Version
        $manifestMinVersion = [version][string]$vclibs[0].MinVersion
        if ($packageVersion -lt $manifestMinVersion) { Fail "staged VCLibs package version $packageVersion is below manifest dependency MinVersion $manifestMinVersion." }
    } finally {
        if (Test-Path -LiteralPath $vclibsUnpack) { Remove-Item -LiteralPath $vclibsUnpack -Recurse -Force -ErrorAction SilentlyContinue }
    }
}

$sourceFiles = @(Get-ChildItem -LiteralPath $projectRoot -Recurse -File -Include *.c,*.cc,*.cpp,*.cxx -ErrorAction SilentlyContinue)
if ($sourceFiles.Count -eq 0) {
    Fail "target project directory contains no native xemu source file."
}

[pscustomobject]@{
    Ready = $true
    Project = $projectRelative.Replace('\', '/')
    Manifest = (Get-RelativePath $RepositoryRoot $manifestPath).Replace('\', '/')
    PackageName = [string]$identity.Name
    Publisher = [string]$identity.Publisher
    Architecture = [string]$identity.ProcessorArchitecture
    ApplicationId = [string]$application.Id
} | ConvertTo-Json -Compress | Write-Output
