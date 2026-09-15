param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [string]$CertificatePath,
    [string]$CertificatePassword,
    [string]$VCLibsPackagePath,
    [switch]$UseMsbuild
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Msbuild = Get-Command msbuild.exe -ErrorAction SilentlyContinue
if ($UseMsbuild -and $Msbuild) {
    & $Msbuild.Source (Join-Path $Root "xemu-uwp-host-probe.vcxproj") "/p:Configuration=$Configuration" "/p:Platform=$Platform" /t:Build
} else {
    # VS installations without the UWP MSBuild targets can still build this
    # probe through the SDK/MSVC-only driver.
    & (Join-Path $PSScriptRoot "build-direct.ps1") -Configuration $Configuration -Platform $Platform
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$SdkRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin"
$MakeAppx = Get-ChildItem $SdkRoot -Filter makeappx.exe -Recurse -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\x64\\makeappx\.exe$' } |
    Sort-Object FullName -Descending | Select-Object -First 1
if (-not $MakeAppx) { throw "MakeAppx.exe was not found in the installed Windows SDKs." }

function Get-AppxIdentity {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)]$MakeAppx
    )
    $resolved = (Resolve-Path -LiteralPath $Path -ErrorAction Stop).Path
    if ([IO.Path]::GetExtension($resolved).ToLowerInvariant() -notin @(".appx", ".msix")) {
        throw "Only .appx and .msix dependencies are supported: $resolved"
    }
    $unpack = Join-Path ([IO.Path]::GetTempPath()) ("xemu-uwp-dependency-" + [guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force $unpack | Out-Null
    try {
        & $MakeAppx.FullName unpack /p $resolved /d $unpack /o | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "MakeAppx could not unpack dependency: $resolved" }
        $manifestPath = Join-Path $unpack "AppxManifest.xml"
        if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
            throw "Dependency does not contain AppxManifest.xml: $resolved"
        }
        [xml]$manifest = Get-Content -LiteralPath $manifestPath -Raw
        $identity = $manifest.SelectSingleNode("/*[local-name()='Package']/*[local-name()='Identity']")
        if (-not $identity) { throw "Dependency manifest has no Identity: $resolved" }
        [pscustomobject]@{
            Path = $resolved
            Manifest = $manifest
            Identity = $identity
        }
    } finally {
        if (Test-Path -LiteralPath $unpack) {
            Remove-Item -LiteralPath $unpack -Recurse -Force
        }
    }
}

$mainManifestPath = Join-Path $Root "Package.appxmanifest"
[xml]$mainManifest = Get-Content -LiteralPath $mainManifestPath -Raw
$declaredDependency = $mainManifest.SelectSingleNode(
    "/*[local-name()='Package']/*[local-name()='Dependencies']/*[local-name()='PackageDependency']")
if (-not $declaredDependency) {
    throw "Package.appxmanifest must declare the Microsoft.VCLibs framework dependency."
}
$mainIdentity = $mainManifest.SelectSingleNode(
    "/*[local-name()='Package']/*[local-name()='Identity']")
if (-not $mainIdentity -or [string]$mainIdentity.ProcessorArchitecture -ne "x64") {
    throw "Package.appxmanifest Identity must declare ProcessorArchitecture=x64."
}

if (-not $VCLibsPackagePath) {
    throw "-VCLibsPackagePath is required. Supply the official signed Microsoft.VCLibs x64 .appx beside the generated MSIX; it is not committed to the repository."
}
$dependency = Get-AppxIdentity -Path $VCLibsPackagePath -MakeAppx $MakeAppx
$dependencyIdentity = $dependency.Identity
$expectedVCLibsName = "Microsoft.VCLibs.140.00"
$expectedVCLibsPublisher = "CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US"
$expectedVCLibsMinVersion = [version]"14.0.33519.0"
if ([string]$dependencyIdentity.Name -ne $expectedVCLibsName -or
    [string]$dependencyIdentity.ProcessorArchitecture -ne "x64" -or
    [string]$dependencyIdentity.Publisher -ne $expectedVCLibsPublisher) {
    throw "VCLibs dependency identity does not match the x64 Microsoft framework package: $($dependency.Path)"
}
if ([version]$dependencyIdentity.Version -lt $expectedVCLibsMinVersion) {
    throw "VCLibs dependency $($dependencyIdentity.Version) is older than the declared minimum $expectedVCLibsMinVersion."
}
if ([string]$declaredDependency.Name -ne [string]$dependencyIdentity.Name -or
    [string]$declaredDependency.Publisher -ne [string]$dependencyIdentity.Publisher -or
    [version]$declaredDependency.MinVersion -ne $expectedVCLibsMinVersion) {
    throw "Package.appxmanifest dependency does not match the inspected Microsoft.VCLibs manifest."
}

$SignTool = Get-ChildItem $SdkRoot -Filter signtool.exe -Recurse -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\x64\\signtool\.exe$' } |
    Sort-Object FullName -Descending | Select-Object -First 1
if (-not $SignTool) { throw "SignTool.exe was not found; dependency signature cannot be verified." }
& $SignTool.FullName verify /pa /all $dependency.Path | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Microsoft.VCLibs dependency signature verification failed: $($dependency.Path)" }

$Output = Join-Path $Root "AppPackages\xemu-uwp-host-probe_$Configuration`_$Platform"
$Stage = Join-Path $Output "Package"
if (Test-Path $Stage) { Remove-Item $Stage -Recurse -Force }
New-Item -ItemType Directory -Force (Join-Path $Stage "assets") | Out-Null
$Exe = Join-Path $Root "bin\$Platform\$Configuration\xemu-uwp-host-probe.exe"
if (-not (Test-Path $Exe)) { throw "Expected build output not found: $Exe" }
$ManifestText = Get-Content (Join-Path $Root "Package.appxmanifest") -Raw
$ManifestText = $ManifestText.Replace('Executable="$targetnametoken$.exe"', 'Executable="xemu-uwp-host-probe.exe"')
Set-Content -Path (Join-Path $Stage "AppxManifest.xml") -Value $ManifestText -Encoding UTF8
Copy-Item $Exe (Join-Path $Stage "xemu-uwp-host-probe.exe")

# Generate package-local raster logos. No certificate or private key is generated here.
Add-Type -AssemblyName System.Drawing
function New-LogoPng([string]$Path, [int]$Size) {
    $bitmap = New-Object System.Drawing.Bitmap($Size, $Size)
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try {
            $graphics.Clear([System.Drawing.Color]::FromArgb(32, 32, 32))
            $brush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(0, 216, 192))
            try { $graphics.FillEllipse($brush, [int]($Size * .18), [int]($Size * .18), [int]($Size * .64), [int]($Size * .64)) }
            finally { $brush.Dispose() }
        } finally { $graphics.Dispose() }
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally { $bitmap.Dispose() }
}
New-LogoPng (Join-Path $Stage "assets\Logo.png") 150

$Package = Join-Path $Output "xemu-uwp-host-probe.msix"
& $MakeAppx.FullName pack /d $Stage /p $Package /o
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$DependencyOutput = Join-Path $Output (Split-Path -Leaf $dependency.Path)
if ([IO.Path]::GetFullPath($dependency.Path) -ne [IO.Path]::GetFullPath($DependencyOutput)) {
    Copy-Item -LiteralPath $dependency.Path -Destination $DependencyOutput -Force
}
Write-Host "Copied verified Microsoft.VCLibs dependency to $DependencyOutput"
if ($CertificatePath) {
    if (-not (Test-Path $CertificatePath -PathType Leaf)) { throw "Certificate file not found: $CertificatePath" }
    $SignTool = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if (-not $SignTool) {
        $SignTool = Get-ChildItem $SdkRoot -Filter signtool.exe -Recurse -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match '\\x64\\signtool\.exe$' } |
            Sort-Object FullName -Descending | Select-Object -First 1
    }
    if (-not $SignTool) { throw "SignTool.exe was not found; provide a Windows SDK installation." }
    $SignArgs = @("sign", "/fd", "SHA256", "/f", (Resolve-Path $CertificatePath).Path)
    if ($CertificatePassword) { $SignArgs += @("/p", $CertificatePassword) }
    $SignArgs += $Package
    $SignToolPath = if ($SignTool.PSObject.Properties.Name -contains "Source") { $SignTool.Source } else { $SignTool.FullName }
    & $SignToolPath @SignArgs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
