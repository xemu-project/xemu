[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ArchiveRoot,
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,
    [string]$SdkVersion = "10.0.26100.0",
    [version]$RequiredMinVersion = [version]"14.0.33519.0"
)

$ErrorActionPreference = "Stop"
$frameworkFloor = [version]"14.0.33519.0"
$effectiveMinVersion = if ($RequiredMinVersion -gt $frameworkFloor) { $RequiredMinVersion } else { $frameworkFloor }

function Fail([string]$Message) {
    throw "xemu VCLibs selection failed: $Message"
}

$ArchiveRoot = (Resolve-Path -LiteralPath $ArchiveRoot -ErrorAction Stop).Path
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$sdkBin = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin\$SdkVersion\x64"
$makeAppx = Join-Path $sdkBin "makeappx.exe"
$signtool = Join-Path $sdkBin "signtool.exe"
foreach ($tool in @($makeAppx, $signtool)) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) { Fail "Windows SDK tool not found: $tool" }
}

$candidates = @(Get-ChildItem -LiteralPath $ArchiveRoot -Recurse -File -Filter "*.appx")
$selectedMatches = [System.Collections.Generic.List[object]]::new()
foreach ($candidate in $candidates) {
    $inspect = Join-Path ([IO.Path]::GetTempPath()) ("xemu-vclibs-" + [guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $inspect | Out-Null
    try {
        & $makeAppx unpack /p $candidate.FullName /d $inspect /o | Out-Null
        if ($LASTEXITCODE -ne 0) { continue }
        $manifestPath = Join-Path $inspect "AppxManifest.xml"
        if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { continue }
        [xml]$manifest = Get-Content -LiteralPath $manifestPath -Raw
        $identity = $manifest.SelectSingleNode("/*[local-name()='Package']/*[local-name()='Identity']")
        if (-not $identity) { continue }
        $name = [string]$identity.Name
        if ($name -eq "Microsoft.VCLibs.140.00.UWPDesktop") {
            # The pinned Microsoft dependency archive intentionally carries
            # both VCLibs identities. Skip UWPDesktop; it is never a valid
            # Xbox dependency and is rejected again during bundle assembly.
            continue
        }
        if ($name -eq "Microsoft.VCLibs.140.00" -and
            [string]$identity.ProcessorArchitecture -eq "x64" -and
            [string]$identity.Publisher -eq "CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US" -and
            [version][string]$identity.Version -ge $effectiveMinVersion) {
            $selectedMatches.Add([pscustomobject]@{ Path = $candidate.FullName; Name = $name; Version = [version][string]$identity.Version })
        }
    } finally {
        if (Test-Path -LiteralPath $inspect) { Remove-Item -LiteralPath $inspect -Recurse -Force -ErrorAction SilentlyContinue }
    }
}
if ($selectedMatches.Count -ne 1) {
    Fail "expected exactly one x64 Microsoft.VCLibs.140.00 package at or above $effectiveMinVersion; found $($selectedMatches.Count). UWPDesktop and all other framework identities are rejected."
}
$selected = $selectedMatches[0]
& $signtool verify /pa /all $selected.Path
if ($LASTEXITCODE -ne 0) { Fail "selected Microsoft.VCLibs signature verification failed." }
$destination = Join-Path $OutputDirectory (Split-Path -Leaf $selected.Path)
Copy-Item -LiteralPath $selected.Path -Destination $destination -Force
Write-Output $destination
