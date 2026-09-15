[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDirectory,
    [string]$ReportPath
)

$ErrorActionPreference = "Stop"

function Fail([string]$Message) {
    throw "xemu Xbox UWP test gate failed: $Message"
}

function Get-RelativePath([string]$BasePath, [string]$TargetPath) {
    $baseUri = New-Object Uri(([IO.Path]::GetFullPath($BasePath).TrimEnd('\') + '\'))
    $targetUri = New-Object Uri([IO.Path]::GetFullPath($TargetPath))
    return [Uri]::UnescapeDataString($baseUri.MakeRelativeUri($targetUri).ToString()).Replace('/', '\')
}

$BuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory -ErrorAction Stop).Path
if (-not $env:GITHUB_WORKSPACE) {
    Fail "GITHUB_WORKSPACE is required so the test build cannot be redirected to an untrusted pre-existing directory."
}
$workspace = (Resolve-Path -LiteralPath $env:GITHUB_WORKSPACE -ErrorAction Stop).Path
$expectedBuildDirectory = [IO.Path]::GetFullPath((Join-Path $workspace "build-xbox-uwp-tests"))
if (-not [StringComparer]::OrdinalIgnoreCase.Equals($BuildDirectory, $expectedBuildDirectory)) {
    Fail "BuildDirectory must be the clean workflow-owned '$expectedBuildDirectory'."
}
$testNames = @(
    "test-vsh-cpu-transform.exe",
    "test-psh-interpreter.exe",
    "test-psh-interpreter-warp.exe",
    "test-d3d11-present.exe",
    "test-d3d11-corewindow-present.exe",
    "test-d3d11-clear.exe",
    "test-d3d11-clear-adapter.exe",
    "test-d3d11-clear-executor.exe",
    "test-d3d11-draw.exe",
    "test-d3d11-draw-executor.exe",
    "test-d3d11-surface.exe",
    "test-d3d11-state-adapter.exe",
    "test-d3d11-vertex-adapter.exe"
)
$results = [System.Collections.Generic.List[object]]::new()
foreach ($name in $testNames) {
    $matches = @(Get-ChildItem -LiteralPath $BuildDirectory -Recurse -File -Filter $name -ErrorAction SilentlyContinue)
    if ($matches.Count -ne 1) {
        Fail "expected exactly one built '$name' below '$BuildDirectory'; found $($matches.Count). Configure/build the complete Windows D3D11 test set."
    }
    $path = $matches[0].FullName
    Write-Host "Running $name"
    & $path --tap -k
    $exitCode = $LASTEXITCODE
    $results.Add([pscustomobject]@{
        Name = $name
        Path = (Get-RelativePath $BuildDirectory $path).Replace('\', '/')
        ExitCode = $exitCode
        Passed = ($exitCode -eq 0)
    })
    if ($exitCode -ne 0) { Fail "test '$name' failed with exit code $exitCode." }
}
if (-not $ReportPath) { $ReportPath = Join-Path $BuildDirectory "xbox-uwp-test-results.json" }
$results | ConvertTo-Json | Set-Content -LiteralPath $ReportPath -Encoding UTF8
Write-Host "All $($testNames.Count) Xbox UWP CPU/D3D11/WARP tests passed."
