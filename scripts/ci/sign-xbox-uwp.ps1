[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$PackagePath,
    [Parameter(Mandatory = $true)]
    [ValidateSet("pfx_required", "dev_test_certificate")]
    [string]$Mode,
    [string]$PfxPath,
    [string]$PfxPassword,
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,
    [string]$VclibsPackagePath,
    [string]$SdkVersion = "10.0.26100.0"
)

$ErrorActionPreference = "Stop"

function Fail([string]$Message) {
    throw "xemu Xbox UWP signing failed: $Message"
}

$PackagePath = (Resolve-Path -LiteralPath $PackagePath -ErrorAction Stop).Path
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$sdkBin = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin\$SdkVersion\x64"
$makeAppx = Join-Path $sdkBin "makeappx.exe"
$signtool = Join-Path $sdkBin "signtool.exe"
foreach ($tool in @($makeAppx, $signtool)) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) { Fail "Windows SDK tool not found: $tool" }
}

$unpack = Join-Path ([IO.Path]::GetTempPath()) ("xemu-xbox-package-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $unpack | Out-Null
$cert = $null
$generated = $false
$temporaryRootCert = $null
$vclibsUnpack = $null
try {
    & $makeAppx unpack /p $PackagePath /d $unpack /o | Out-Null
    if ($LASTEXITCODE -ne 0) { Fail "MakeAppx could not unpack the unsigned package." }
    $manifestPath = Join-Path $unpack "AppxManifest.xml"
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { Fail "package has no AppxManifest.xml." }
    [xml]$manifest = Get-Content -LiteralPath $manifestPath -Raw
    $identity = $manifest.SelectSingleNode("/*[local-name()='Package']/*[local-name()='Identity']")
    if (-not $identity) { Fail "package manifest has no Identity." }
    if ([string]$identity.ProcessorArchitecture -ne "x64") { Fail "package architecture is not x64." }
    if ([string]$identity.Name -eq "xemu.UwpHostProbe") { Fail "host-probe package cannot be signed as the full emulator release." }
    $publisher = [string]$identity.Publisher
    if ([string]::IsNullOrWhiteSpace($publisher) -or $publisher -match '(?i)development|placeholder|example') {
        Fail "package publisher '$publisher' is not a stable signing publisher."
    }
    $families = @($manifest.SelectNodes("/*[local-name()='Package']/*[local-name()='Dependencies']/*[local-name()='TargetDeviceFamily']"))
    $xboxFamilies = @($families | Where-Object { [string]$_.Name -eq 'Windows.Xbox' })
    if ($xboxFamilies.Count -ne 1) { Fail 'signed package must declare exactly one Windows.Xbox TargetDeviceFamily.' }
    foreach ($family in $families) {
        if ([string]$family.Name -notin @('Windows.Xbox', 'Windows.Universal')) { Fail "unsupported TargetDeviceFamily '$($family.Name)'." }
        try {
            $min = [version][string]$family.MinVersion
            $max = [version][string]$family.MaxVersionTested
        } catch { Fail "TargetDeviceFamily '$($family.Name)' has invalid version attributes." }
        if ($min -lt [version]'10.0.14393.0' -or $max -lt $min) { Fail "TargetDeviceFamily '$($family.Name)' has invalid MinVersion/MaxVersionTested." }
    }
    $application = $manifest.SelectSingleNode("/*[local-name()='Package']/*[local-name()='Applications']/*[local-name()='Application']")
    if (-not $application -or [string]$application.Executable -match '\$targetnametoken\$' -or [string]$application.Executable -notmatch '(?i)\.exe$' -or [string]::IsNullOrWhiteSpace([string]$application.EntryPoint)) {
        Fail 'signed package must contain an explicit executable and non-empty EntryPoint.'
    }
    $dependencies = @($manifest.SelectNodes("/*[local-name()='Package']/*[local-name()='Dependencies']/*[local-name()='PackageDependency']"))
    if (@($dependencies | Where-Object { [string]$_.Name -match '(?i)UWPDesktop' }).Count -ne 0) { Fail 'Microsoft.VCLibs.140.00.UWPDesktop is forbidden.' }
    $vclibs = @($dependencies | Where-Object { [string]$_.Name -eq 'Microsoft.VCLibs.140.00' })
    if ($vclibs.Count -ne 1 -or [string]$vclibs[0].Publisher -ne 'CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US' -or [version][string]$vclibs[0].MinVersion -lt [version]'14.0.33519.0') {
        Fail 'signed package must declare the official Microsoft.VCLibs.140.00 dependency.'
    }
    if (-not $VclibsPackagePath) { Fail 'the exact staged Microsoft.VCLibs.140.00 package is required for version matching.' }
    $VclibsPackagePath = (Resolve-Path -LiteralPath $VclibsPackagePath -ErrorAction Stop).Path
    $vclibsUnpack = Join-Path ([IO.Path]::GetTempPath()) ("xemu-vclibs-package-" + [guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $vclibsUnpack | Out-Null
    & $makeAppx unpack /p $VclibsPackagePath /d $vclibsUnpack /o | Out-Null
    if ($LASTEXITCODE -ne 0) { Fail 'MakeAppx could not unpack the staged VCLibs package.' }
    $vclibsManifest = Join-Path $vclibsUnpack 'AppxManifest.xml'
    if (-not (Test-Path -LiteralPath $vclibsManifest -PathType Leaf)) { Fail 'staged VCLibs package has no AppxManifest.xml.' }
    [xml]$vclibsPackageManifest = Get-Content -LiteralPath $vclibsManifest -Raw
    $vclibsIdentity = $vclibsPackageManifest.SelectSingleNode("/*[local-name()='Package']/*[local-name()='Identity']")
    if (-not $vclibsIdentity -or [string]$vclibsIdentity.Name -ne 'Microsoft.VCLibs.140.00' -or [string]$vclibsIdentity.ProcessorArchitecture -ne 'x64' -or [string]$vclibsIdentity.Publisher -ne 'CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US') {
        Fail 'staged VCLibs package identity is not the official x64 Microsoft.VCLibs.140.00 package.'
    }
    $manifestMinVersion = [version][string]$vclibs[0].MinVersion
    $packageVclibsVersion = [version][string]$vclibsIdentity.Version
    if ($packageVclibsVersion -lt $manifestMinVersion) {
        Fail "staged VCLibs package version $packageVclibsVersion is below manifest dependency MinVersion $manifestMinVersion."
    }

    if ($Mode -eq "pfx_required") {
        if (-not $PfxPath) { Fail "-PfxPath is required when -Mode pfx_required is selected." }
        if (-not (Test-Path -LiteralPath $PfxPath -PathType Leaf)) { Fail "PFX file was not found: $PfxPath" }
        $plainPassword = if ($PfxPassword) { $PfxPassword } else { "" }
        $pfxPasswordSecure = ConvertTo-SecureString $plainPassword -AsPlainText -Force
        $pfxData = Get-PfxData -FilePath $PfxPath -Password $pfxPasswordSecure
        $cert = $pfxData.EndEntityCertificates | Select-Object -First 1
        if (-not $cert) { Fail "the supplied PFX contains no end-entity certificate." }
    } else {
        # This path is intentionally for Xbox Developer Mode sideloading only.
        # The public certificate is emitted beside the package so Device Portal
        # can install it; this is not a Store/production signing identity.
        $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject $publisher -CertStoreLocation "Cert:\CurrentUser\My" -KeyExportPolicy Exportable -NotAfter (Get-Date).AddDays(30)
        if (-not $cert) { Fail "New-SelfSignedCertificate did not return a certificate." }
        $generated = $true
        $PfxPath = Join-Path $OutputDirectory "xbox-dev-test-signing.pfx"
        $PfxPassword = [guid]::NewGuid().ToString("N")
        $pfxPasswordSecure = ConvertTo-SecureString $PfxPassword -AsPlainText -Force
        Export-PfxCertificate -Cert $cert -FilePath $PfxPath -Password $pfxPasswordSecure | Out-Null
    }
    $now = Get-Date
    if ($cert.NotBefore -gt $now -or $cert.NotAfter -le $now) {
        Fail "signing certificate is outside its validity window."
    }
    $codeSigningEku = @($cert.Extensions | Where-Object {
        $_.Oid.Value -eq '1.3.6.1.5.5.7.3.3'
    })
    if ($codeSigningEku.Count -ne 1) {
        Fail "signing certificate must contain exactly one Code Signing EKU (1.3.6.1.5.5.7.3.3)."
    }
    if ($cert.Subject -ne $publisher) {
        Fail "certificate subject '$($cert.Subject)' does not exactly match manifest publisher '$publisher'."
    }

    $publicCertificate = Join-Path $OutputDirectory "xbox-devmode-signing.cer"
    Export-Certificate -Cert $cert -FilePath $publicCertificate -Type CERT | Out-Null
    # Trust the public certificate only for this verification process. This
    # makes signtool verification meaningful for both a supplied Dev Mode PFX
    # and the explicitly labelled ephemeral test certificate.
    $temporaryRootCert = Import-Certificate -FilePath $publicCertificate -CertStoreLocation "Cert:\CurrentUser\Root"
    $signArgs = @("sign", "/fd", "SHA256", "/f", $PfxPath)
    if ($PfxPassword) { $signArgs += @("/p", $PfxPassword) }
    $signArgs += $PackagePath
    & $signtool @signArgs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $makeAppx validate /p $PackagePath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $signtool verify /pa /all $PackagePath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    [pscustomobject]@{
        Mode = $Mode
        Package = [IO.Path]::GetFileName($PackagePath)
        Publisher = $publisher
        CertificateSubject = $cert.Subject
        CertificateThumbprint = $cert.Thumbprint
        CertificateFile = [IO.Path]::GetFileName($publicCertificate)
        PackageSha256 = (Get-FileHash -LiteralPath $PackagePath -Algorithm SHA256).Hash
        XboxDevModeOnly = ($Mode -eq "dev_test_certificate")
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputDirectory "signing-metadata.json") -Encoding UTF8
} finally {
    if ($temporaryRootCert) {
        Remove-Item -LiteralPath ("Cert:\CurrentUser\Root\" + $temporaryRootCert.Thumbprint) -Force -ErrorAction SilentlyContinue
    }
    if ($generated -and $cert) {
        Remove-Item -LiteralPath ("Cert:\CurrentUser\My\" + $cert.Thumbprint) -Force -ErrorAction SilentlyContinue
        if ($PfxPath -and (Test-Path -LiteralPath $PfxPath)) {
            # The ephemeral private key must never leave the runner artifact.
            Remove-Item -LiteralPath $PfxPath -Force -ErrorAction SilentlyContinue
        }
    }
    if ($PfxPath) {
        $tempDirectory = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { [IO.Path]::GetTempPath() }
        $tempRoot = [IO.Path]::GetFullPath($tempDirectory).TrimEnd('\') + '\'
        $pfxFullPath = [IO.Path]::GetFullPath($PfxPath)
        if ($pfxFullPath.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -and
            (Test-Path -LiteralPath $pfxFullPath)) {
            Remove-Item -LiteralPath $pfxFullPath -Force -ErrorAction SilentlyContinue
        }
    }
    if (Test-Path -LiteralPath $unpack) { Remove-Item -LiteralPath $unpack -Recurse -Force -ErrorAction SilentlyContinue }
    if ($vclibsUnpack -and (Test-Path -LiteralPath $vclibsUnpack)) { Remove-Item -LiteralPath $vclibsUnpack -Recurse -Force -ErrorAction SilentlyContinue }
}
