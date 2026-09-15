[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$PackagePath,

    [Parameter(Mandatory = $false)]
    [uri]$DevicePortalUri,

    [Parameter(Mandatory = $false)]
    [System.Management.Automation.PSCredential]$Credential,

    [Parameter(Mandatory = $false)]
    [string]$CertificatePath,

    [Parameter(Mandatory = $false)]
    [string[]]$DependencyPath,

    [Parameter(Mandatory = $false)]
    [ValidatePattern("^[0-9a-fA-F]{64}$")]
    [string]$ExpectedSha256,

    [Parameter(Mandatory = $false)]
    [switch]$Deploy,

    [Parameter(Mandatory = $false)]
    [switch]$Launch,

    [Parameter(Mandatory = $false)]
    [switch]$Telemetry,

    [Parameter(Mandatory = $false)]
    [string]$AppId = "App",

    [Parameter(Mandatory = $false)]
    [string]$TelemetryOutputPath,

    [Parameter(Mandatory = $false)]
    [switch]$SkipCertificateCheck,

    [Parameter(Mandatory = $false)]
    [ValidateRange(10, 1800)]
    [int]$InstallTimeoutSeconds = 120
)

$ErrorActionPreference = "Stop"

function Get-PackageDetails {
    param([string]$Path)

    $resolved = (Resolve-Path -LiteralPath $Path -ErrorAction Stop).Path
    if ([IO.Path]::GetExtension($resolved).ToLowerInvariant() -notin @(".appx", ".msix")) {
        throw "Only .appx and .msix packages are supported: $resolved"
    }

    $sha256 = (Get-FileHash -LiteralPath $resolved -Algorithm SHA256).Hash
    $temp = Join-Path ([IO.Path]::GetTempPath()) ("xemu-probe-" + [guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $temp | Out-Null
    try {
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        [IO.Compression.ZipFile]::ExtractToDirectory($resolved, $temp)
        $manifestPath = Join-Path $temp "AppxManifest.xml"
        if (-not (Test-Path -LiteralPath $manifestPath)) { throw "Package does not contain AppxManifest.xml." }
        [xml]$manifest = Get-Content -LiteralPath $manifestPath -Raw
        $identity = $manifest.Package.Identity
        if (-not $identity -or [string]::IsNullOrWhiteSpace($identity.Publisher)) {
            throw "Package manifest has no publisher identity."
        }
        if ([string]$identity.ProcessorArchitecture -ne "x64") {
            throw "Package manifest ProcessorArchitecture must be x64."
        }
        $signature = Get-AuthenticodeSignature -FilePath $resolved
        if ($signature.Status -ne "Valid") {
            throw "Package signature is not valid ($($signature.Status)). Sign the package before deployment."
        }

        $certificate = $null
        if ($CertificatePath) {
            $certResolved = (Resolve-Path -LiteralPath $CertificatePath -ErrorAction Stop).Path
            $certificate = Get-PfxCertificate -FilePath $certResolved
            if ($certificate.Subject -ne [string]$identity.Publisher) {
                throw "Certificate subject '$($certificate.Subject)' does not match manifest publisher '$($identity.Publisher)'."
            }
            if ($signature.SignerCertificate -and
                $signature.SignerCertificate.Thumbprint -ne $certificate.Thumbprint) {
                throw "Package signer thumbprint does not match -CertificatePath."
            }
        }

        $packageDependencies = @($manifest.SelectNodes("/*[local-name()='Package']/*[local-name()='Dependencies']/*[local-name()='PackageDependency']") |
            ForEach-Object {
                [pscustomobject]@{
                    Name = [string]$_.Name
                    Publisher = [string]$_.Publisher
                    MinVersion = [version]$_.MinVersion
                }
            })

        [pscustomobject]@{
            Path = $resolved
            Sha256 = $sha256
            Name = [string]$identity.Name
            Publisher = [string]$identity.Publisher
            Version = [string]$identity.Version
            ProcessorArchitecture = [string]$identity.ProcessorArchitecture
            Dependencies = $packageDependencies
            SignatureStatus = [string]$signature.Status
            SignerSubject = if ($signature.SignerCertificate) { $signature.SignerCertificate.Subject } else { $null }
            SignerThumbprint = if ($signature.SignerCertificate) { $signature.SignerCertificate.Thumbprint } else { $null }
        }
    } finally {
        if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Recurse -Force }
    }
}

function Resolve-DeclaredDependencies {
    param(
        [Parameter(Mandatory = $true)]$PackageDetails,
        [string[]]$RequestedPaths
    )

    $declared = @($PackageDetails.Dependencies)
    if ($declared.Count -eq 0) { return @() }

    $candidatePaths = @($RequestedPaths | Where-Object { $_ })
    if ($candidatePaths.Count -eq 0) {
        $packageDirectory = Split-Path -Parent $PackageDetails.Path
        $candidatePaths = @(Get-ChildItem -LiteralPath $packageDirectory -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Extension.ToLowerInvariant() -in @(".appx", ".msix") -and
                           $_.FullName -ne $PackageDetails.Path } |
            Select-Object -ExpandProperty FullName)
        if ($candidatePaths.Count -eq 0) {
            throw "Package declares framework dependencies, but no adjacent .appx/.msix dependency was found."
        }
        Write-Host "No -DependencyPath supplied; resolving declared dependencies beside the package."
    }

    $resolved = @()
    foreach ($candidatePath in $candidatePaths) {
        $candidate = Get-PackageDetails -Path $candidatePath
        $resolved += $candidate
    }
    foreach ($dependency in $declared) {
        $match = @($resolved | Where-Object {
            $_.Name -eq $dependency.Name -and
            $_.Publisher -eq $dependency.Publisher -and
            ([version]$_.Version -ge $dependency.MinVersion)
        })
        if ($match.Count -ne 1) {
            throw "Could not resolve exactly one signed dependency for $($dependency.Name) >= $($dependency.MinVersion)."
        }
    }
    foreach ($candidate in $resolved) {
        Write-Host ("Dependency: {0} {1} Publisher={2}" -f $candidate.Path, $candidate.Version, $candidate.Publisher)
        Write-Host ("Dependency SHA-256: {0}" -f $candidate.Sha256)
    }
    return @($resolved | Select-Object -ExpandProperty Path)
}

function Invoke-PortalRequest {
    param(
        [ValidateSet("GET", "POST")][string]$Method,
        [string]$Uri,
        [System.Management.Automation.PSCredential]$RequestCredential,
        [object]$Body,
        [string]$ContentType = "application/json"
    )
    $params = @{ Method = $Method; Uri = $Uri; UseBasicParsing = $true; ErrorAction = "Stop" }
    if ($RequestCredential) { $params.Credential = $RequestCredential }
    if ($null -ne $Body) { $params.Body = $Body; $params.ContentType = $ContentType }
    if ($SkipCertificateCheck -and ((Get-Command Invoke-WebRequest).Parameters.ContainsKey("SkipCertificateCheck"))) {
        $params.SkipCertificateCheck = $true
    }
    Invoke-WebRequest @params
}

function Get-ResponseObject {
    param($Response)
    if ([string]::IsNullOrWhiteSpace($Response.Content)) { return $null }
    try { return ($Response.Content | ConvertFrom-Json) } catch { return $null }
}

function Add-QueryParameter {
    param([string]$Uri, [string]$Name, [string]$Value)
    $separator = if ($Uri.Contains("?")) { "&" } else { "?" }
    return "$Uri$separator$([uri]::EscapeDataString($Name))=$([uri]::EscapeDataString($Value))"
}

function Invoke-MultipartUpload {
    param(
        [string]$Uri,
        [System.Management.Automation.PSCredential]$RequestCredential,
        [string]$PackagePath,
        [string[]]$DependencyPaths,
        [string]$CertificatePath
    )

    # Device Portal expects a multipart/form-data request. The query parameters
    # identify the package/dependencies/certificate while the parts carry bytes.
    $content = New-Object System.Net.Http.MultipartFormDataContent
    $streams = New-Object 'System.Collections.Generic.List[System.IO.Stream]'
    try {
        $packageStream = [IO.File]::OpenRead($PackagePath)
        [void]$streams.Add($packageStream)
        $packagePart = New-Object -TypeName System.Net.Http.StreamContent -ArgumentList $packageStream
        $packagePart.Headers.ContentType = [System.Net.Http.Headers.MediaTypeHeaderValue]::Parse("application/octet-stream")
        $content.Add($packagePart, "package", [IO.Path]::GetFileName($PackagePath))

        foreach ($dependency in @($DependencyPaths)) {
            $dependencyStream = [IO.File]::OpenRead($dependency)
            [void]$streams.Add($dependencyStream)
            $dependencyPart = New-Object -TypeName System.Net.Http.StreamContent -ArgumentList $dependencyStream
            $dependencyPart.Headers.ContentType = [System.Net.Http.Headers.MediaTypeHeaderValue]::Parse("application/octet-stream")
            $content.Add($dependencyPart, "dependency", [IO.Path]::GetFileName($dependency))
        }
        if ($CertificatePath) {
            $certificateStream = [IO.File]::OpenRead($CertificatePath)
            [void]$streams.Add($certificateStream)
            $certificatePart = New-Object -TypeName System.Net.Http.StreamContent -ArgumentList $certificateStream
            $certificatePart.Headers.ContentType = [System.Net.Http.Headers.MediaTypeHeaderValue]::Parse("application/octet-stream")
            $content.Add($certificatePart, "certificate", [IO.Path]::GetFileName($CertificatePath))
        }

        $bytes = $content.ReadAsByteArrayAsync().GetAwaiter().GetResult()
        $contentType = $content.Headers.ContentType.ToString()
        return Invoke-PortalRequest -Method POST -Uri $Uri -RequestCredential $RequestCredential -Body $bytes -ContentType $contentType
    } finally {
        $content.Dispose()
        foreach ($stream in $streams) { $stream.Dispose() }
    }
}

function Get-PackageFullName {
    param($Object, [string]$PackageName)
    if ($Object) {
        foreach ($property in @("PackageFullName", "packageFullName", "FullName", "fullName")) {
            if ($Object.PSObject.Properties.Name -contains $property -and $Object.$property) {
                return [string]$Object.$property
            }
        }
    }
    return $null
}

function Get-InstalledPackages {
    param($Object)
    if ($Object -and $Object.PSObject.Properties.Name -contains "InstalledPackages") {
        return @($Object.InstalledPackages)
    }
    return @($Object)
}

$details = Get-PackageDetails -Path $PackagePath
$DependencyPath = @(Resolve-DeclaredDependencies -PackageDetails $details -RequestedPaths $DependencyPath)
if ($ExpectedSha256 -and $details.Sha256 -ne $ExpectedSha256.ToUpperInvariant()) {
    throw "SHA-256 mismatch: expected $ExpectedSha256, got $($details.Sha256)."
}
Write-Output ("Package: {0}" -f $details.Path)
Write-Output ("Identity: {0} {1} Publisher={2}" -f $details.Name, $details.Version, $details.Publisher)
Write-Output ("SHA-256: {0}" -f $details.Sha256)
Write-Output ("Signature: {0} ({1})" -f $details.SignatureStatus, $details.SignerSubject)

if (-not $Deploy) {
    Write-Output "Dry run only. Re-run with -Deploy and -DevicePortalUri to upload/install."
    return
}
if (-not $DevicePortalUri) { throw "-DevicePortalUri is required with -Deploy." }
if (-not $Credential) { $Credential = Get-Credential -Message "Xbox Device Portal credentials" }
$base = $DevicePortalUri.AbsoluteUri.TrimEnd("/")
$packageEndpoint = Add-QueryParameter "$base/api/app/packagemanager/package" "package" ([IO.Path]::GetFileName($details.Path))
foreach ($dependency in @($DependencyPath)) {
    $dependencyResolved = (Resolve-Path -LiteralPath $dependency -ErrorAction Stop).Path
    if ([IO.Path]::GetExtension($dependencyResolved).ToLowerInvariant() -notin @(".appx", ".msix")) {
        throw "Only .appx and .msix dependencies are supported: $dependencyResolved"
    }
    $packageEndpoint = Add-QueryParameter $packageEndpoint "dependency" ([IO.Path]::GetFileName($dependencyResolved))
}
if ($CertificatePath) {
    $certificateResolved = (Resolve-Path -LiteralPath $CertificatePath -ErrorAction Stop).Path
    $packageEndpoint = Add-QueryParameter $packageEndpoint "certificate" ([IO.Path]::GetFileName($certificateResolved))
} else {
    $certificateResolved = $null
}

$uploadResponse = Invoke-MultipartUpload -Uri $packageEndpoint -RequestCredential $Credential `
    -PackagePath $details.Path -DependencyPaths $DependencyPath -CertificatePath $certificateResolved
$uploadObject = Get-ResponseObject $uploadResponse
Write-Output ("Device Portal upload returned HTTP {0}." -f [int]$uploadResponse.StatusCode)

$packageFullName = Get-PackageFullName $uploadObject

$deadline = [DateTime]::UtcNow.AddSeconds($InstallTimeoutSeconds)
do {
    $stateResponse = Invoke-PortalRequest -Method GET -Uri "$base/api/app/packagemanager/state" -RequestCredential $Credential
    $state = Get-ResponseObject $stateResponse
    $stateText = if ($state) { ($state | Out-String).Trim() } else { "" }
    $failure = $null
    foreach ($property in @("Error", "error", "Failure", "failure", "ErrorMessage", "errorMessage")) {
        if ($state -and $state.PSObject.Properties.Name -contains $property -and $state.$property) {
            $failure = [string]$state.$property
            break
        }
    }
    if ($failure) { throw "Device Portal package installation failed: $failure" }
    if ($state -and $state.PSObject.Properties.Name -contains "status" -and
        [string]$state.status -match "^(Failed|Error|Canceled|Cancelled)$") {
        throw "Device Portal package installation failed (state: $($state.status))."
    }
    if (-not $packageFullName) { $packageFullName = Get-PackageFullName $state }
    $installing = $state -and (($stateText -match "Installing|Pending|Queued|Processing") -or
        ($state.PSObject.Properties.Name -contains "status" -and [string]$state.status -notmatch "^(Completed|Complete|Installed|Idle|Ready)$"))
    if ($installing -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Seconds 2 }
} while ($installing -and [DateTime]::UtcNow -lt $deadline)
if ($installing) {
    throw "Timed out after $InstallTimeoutSeconds seconds waiting for Device Portal package installation. Last state: $stateText"
}

if (-not $packageFullName) {
    $packagesResponse = Invoke-PortalRequest -Method GET -Uri "$base/api/app/packagemanager/packages" -RequestCredential $Credential
    $installedPackages = Get-InstalledPackages (Get-ResponseObject $packagesResponse)
    foreach ($installed in $installedPackages) {
        $candidate = Get-PackageFullName $installed
        if ($candidate -and $candidate -like "$($details.Name)_*") { $packageFullName = $candidate; break }
    }
}
if (-not $packageFullName) { throw "Device Portal did not return the installed package full name." }

if ($Launch) {
    $launchUri = Add-QueryParameter "$base/api/taskmanager/app" "package" $packageFullName
    $launchUri = Add-QueryParameter $launchUri "appid" $AppId
    $launchResponse = Invoke-PortalRequest -Method POST -Uri $launchUri -RequestCredential $Credential
    Write-Output ("Device Portal launch returned HTTP {0}." -f [int]$launchResponse.StatusCode)
}

if ($Telemetry) {
    $packagesResponse = Invoke-PortalRequest -Method GET -Uri "$base/api/app/packagemanager/packages" -RequestCredential $Credential
    $packages = Get-InstalledPackages (Get-ResponseObject $packagesResponse)
    $candidateNames = @($packageFullName)
    $candidateNames += @($packages | ForEach-Object { Get-PackageFullName $_ })
    $candidateNames = $candidateNames | Where-Object { $_ } | Select-Object -Unique
    $saved = $false
    foreach ($fullName in $candidateNames) {
        $fileUri = "$base/api/filesystem/apps/file"
        $fileUri = Add-QueryParameter $fileUri "package" $fullName
        $fileUri = Add-QueryParameter $fileUri "knownfolderid" "LocalAppData"
        $fileUri = Add-QueryParameter $fileUri "path" "LocalState\xemu-uwp-host-probe.jsonl"
        try {
            $fileResponse = Invoke-PortalRequest -Method GET -Uri $fileUri -RequestCredential $Credential
            $target = if ($TelemetryOutputPath) { $TelemetryOutputPath } else { Join-Path (Get-Location) "xemu-uwp-host-probe.jsonl" }
            $targetParent = Split-Path -Parent $target
            if ($targetParent -and -not (Test-Path -LiteralPath $targetParent)) {
                New-Item -ItemType Directory -Path $targetParent -Force | Out-Null
            }
            [IO.File]::WriteAllText($target, $fileResponse.Content, [Text.Encoding]::UTF8)
            Write-Output "Telemetry saved to $target"
            $saved = $true
            break
        } catch { Write-Warning "Could not download telemetry for package '$fullName': $($_.Exception.Message)" }
    }
    if (-not $saved) { Write-Warning "Telemetry file was not available through Device Portal yet." }
}
