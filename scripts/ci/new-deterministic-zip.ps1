[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SourceDirectory,
    [Parameter(Mandatory = $true)]
    [string]$ZipPath
)

$ErrorActionPreference = "Stop"

function Get-RelativePath([string]$BasePath, [string]$TargetPath) {
    $baseUri = New-Object Uri(([IO.Path]::GetFullPath($BasePath).TrimEnd('\') + '\'))
    $targetUri = New-Object Uri([IO.Path]::GetFullPath($TargetPath))
    return [Uri]::UnescapeDataString($baseUri.MakeRelativeUri($targetUri).ToString()).Replace('/', '\')
}

$SourceDirectory = (Resolve-Path -LiteralPath $SourceDirectory -ErrorAction Stop).Path
$ZipPath = [IO.Path]::GetFullPath($ZipPath)
if ($ZipPath.StartsWith($SourceDirectory.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw "ZipPath must not be inside SourceDirectory."
}
if (Test-Path -LiteralPath $ZipPath) { Remove-Item -LiteralPath $ZipPath -Force }
$parent = Split-Path -Parent $ZipPath
New-Item -ItemType Directory -Force -Path $parent | Out-Null

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$stream = [IO.File]::Open($ZipPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
$archive = New-Object IO.Compression.ZipArchive($stream, [IO.Compression.ZipArchiveMode]::Create, $false)
try {
    $files = @(Get-ChildItem -LiteralPath $SourceDirectory -Recurse -File |
        Sort-Object { (Get-RelativePath $SourceDirectory $_.FullName).Replace('\', '/') })
    foreach ($file in $files) {
        $relative = (Get-RelativePath $SourceDirectory $file.FullName).Replace('\', '/')
        $entry = $archive.CreateEntry($relative, [IO.Compression.CompressionLevel]::Optimal)
        $entry.LastWriteTime = [DateTimeOffset]::new(1980, 1, 1, 0, 0, 0, [TimeSpan]::Zero)
        $input = [IO.File]::OpenRead($file.FullName)
        $output = $entry.Open()
        try { $input.CopyTo($output) } finally { $output.Dispose(); $input.Dispose() }
    }
} finally {
    $archive.Dispose()
    $stream.Dispose()
}
Write-Output $ZipPath
