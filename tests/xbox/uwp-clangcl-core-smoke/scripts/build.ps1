[CmdletBinding()]
param(
    [string]$RepositoryRoot,
    [string]$BuildDirectory,
    [string]$OutputDirectory
)

$ErrorActionPreference = "Stop"

function Fail([string]$Message) {
    throw "xemu UWP clang-cl core smoke prerequisite/build failure: $Message"
}

if (-not $RepositoryRoot) {
    $RepositoryRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
}
$RepositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot -ErrorAction Stop).Path

if (-not $BuildDirectory) {
    $candidates = @(
        (Join-Path $RepositoryRoot "build"),
        (Join-Path $RepositoryRoot "builddir"),
        (Join-Path $RepositoryRoot "build-win32")
    ) | Where-Object { Test-Path -LiteralPath $_ -PathType Container }
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath (Join-Path $candidate "config-host.h") -PathType Leaf) {
            $BuildDirectory = $candidate
            break
        }
    }
}
if (-not $BuildDirectory) {
    Fail "No Meson build directory was supplied. Run a real Windows Meson configure first and pass -BuildDirectory; this smoke test never creates fake config headers."
}
$BuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory -ErrorAction Stop).Path
$ConfigHost = Join-Path $BuildDirectory "config-host.h"
$ConfigMak = Join-Path $BuildDirectory "config-host.mak"
$MesonCoreData = Join-Path $BuildDirectory "meson-private\coredata.dat"
if (-not (Test-Path -LiteralPath $ConfigHost -PathType Leaf) -or
    -not (Test-Path -LiteralPath $ConfigMak -PathType Leaf) -or
    -not (Test-Path -LiteralPath $MesonCoreData -PathType Leaf)) {
    Fail "'$BuildDirectory' is not an authentic current Meson build (config-host.h, config-host.mak, and meson-private\coredata.dat are required)."
}

function Get-MakeVariable([string]$Name) {
    $line = Get-Content -LiteralPath $ConfigMak |
        Where-Object { $_ -match ('^(?:export\s+)?' + [regex]::Escape($Name) + '=') } |
        Select-Object -First 1
    if (-not $line) { Fail "Meson config-host.mak has no $Name assignment." }
    $value = ($line -replace ('^(?:export\s+)?' + [regex]::Escape($Name) + '='), '').Trim()
    return $value.Trim().Trim('"').Trim("'")
}

$ConfiguredSource = [IO.Path]::GetFullPath((Get-MakeVariable "SRC_PATH")).TrimEnd('\')
$ExpectedSource = [IO.Path]::GetFullPath($RepositoryRoot).TrimEnd('\')
if ($ConfiguredSource -ine $ExpectedSource) {
    Fail "config-host.mak SRC_PATH points to '$ConfiguredSource', not the supplied current repository '$ExpectedSource'."
}
$TargetDirs = Get-MakeVariable "TARGET_DIRS"
if (-not (($TargetDirs -split '\s+') -contains "x86_64-softmmu")) {
    Fail "config-host.mak TARGET_DIRS does not contain the required exact x86_64-softmmu target."
}
$ConfigHostText = Get-Content -LiteralPath $ConfigHost -Raw
if ($ConfigHostText -notmatch '(?m)^\s*#pragma once\s*$' -and
    $ConfigHostText -notmatch '(?m)^\s*#define CONFIG_[A-Z0-9_]+') {
    Fail "config-host.h is not a generated QEMU host configuration header."
}

# qemu/osdep.h includes a target config when COMPILING_PER_TARGET is set.
# Require the exact target selected above; do not synthesize or fall back.
$TargetHeaders = @(Get-ChildItem -LiteralPath $BuildDirectory -Filter "x86_64-softmmu-config-target.h" -File -Recurse)
if ($TargetHeaders.Count -ne 1) {
    Fail "Expected exactly one generated x86_64-softmmu-config-target.h below '$BuildDirectory'; found $($TargetHeaders.Count)."
}
$TargetHeader = $TargetHeaders[0]
if ([IO.Path]::GetFullPath($TargetHeader.DirectoryName).TrimEnd('\') -ine
    [IO.Path]::GetFullPath($BuildDirectory).TrimEnd('\')) {
    Fail "x86_64-softmmu-config-target.h must be the configure-generated top-level target header in '$BuildDirectory'."
}
$TargetHeaderText = Get-Content -LiteralPath $TargetHeader.FullName -Raw
foreach ($marker in @("#define TARGET_X86_64 1", "#define TARGET_LONG_BITS 64", "#define TARGET_BIG_ENDIAN 0")) {
    if ($TargetHeaderText -notmatch [regex]::Escape($marker)) {
        Fail "Generated target header '$($TargetHeader.FullName)' is missing '$marker'."
    }
}

function Find-Tool([string]$Name, [string[]]$ExtraDirectories = @()) {
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    foreach ($directory in $ExtraDirectories) {
        if (-not $directory) { continue }
        $candidate = Join-Path $directory $Name
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    }
    return $null
}

$ClangCl = Find-Tool "clang-cl.exe"
if (-not $ClangCl) {
    Fail "clang-cl.exe was not found on PATH. Install/enable an existing LLVM toolchain; this script does not install software."
}
$ClangBin = Split-Path -Parent $ClangCl
$LldLink = Find-Tool "lld-link.exe" @($ClangBin)
if (-not $LldLink) {
    Fail "lld-link.exe was not found beside clang-cl or on PATH. Install/enable the existing LLVM linker; this script does not install software."
}

$VsWhereCandidates = @(
    (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"),
    (Join-Path ${env:ProgramFiles} "Microsoft Visual Studio\Installer\vswhere.exe")
) | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) }
$VsRoot = $null
foreach ($vswhere in $VsWhereCandidates) {
    $VsRoot = (& $vswhere -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -latest -property installationPath 2>$null | Select-Object -First 1)
    if ($VsRoot) { break }
}
if (-not $VsRoot) {
    Fail "Visual Studio VC tools were not found through vswhere.exe. The MSVC ABI smoke requires the Desktop C++ workload."
}
$VsRoot = [string]$VsRoot
$MsvcRoot = Get-ChildItem -LiteralPath (Join-Path $VsRoot "VC\Tools\MSVC") -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending | Select-Object -First 1
if (-not $MsvcRoot) { Fail "No MSVC toolset directory was found below '$VsRoot'." }

$SdkRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10"
$SdkVersion = Get-ChildItem -LiteralPath (Join-Path $SdkRoot "Include") -Directory -ErrorAction SilentlyContinue |
    Where-Object { Test-Path (Join-Path $_.FullName "um\windows.h") } |
    Sort-Object Name -Descending | Select-Object -First 1
if (-not $SdkVersion) { Fail "Windows 10/11 SDK headers were not found under '$SdkRoot'." }
$SdkVersion = $SdkVersion.Name
$SdkInclude = Join-Path $SdkRoot "Include\$SdkVersion"
$SdkLib = Join-Path $SdkRoot "Lib\$SdkVersion"
$CppWinrt = Join-Path $SdkInclude "cppwinrt"
if (-not (Test-Path (Join-Path $CppWinrt "winrt\base.h") -PathType Leaf)) {
    Fail "C++/WinRT headers are missing ('$CppWinrt\winrt\base.h')."
}

$MsvcInclude = Join-Path $MsvcRoot.FullName "include"
$MsvcLib = Join-Path $MsvcRoot.FullName "lib\x64"
$UcrtInclude = Join-Path $SdkInclude "ucrt"
$UmInclude = Join-Path $SdkInclude "um"
$SharedInclude = Join-Path $SdkInclude "shared"
$HostX64Include = Join-Path $RepositoryRoot "host\include\x86_64"
$HostGenericInclude = Join-Path $RepositoryRoot "host\include\generic"
$UcrtLib = Join-Path $SdkLib "ucrt\x64"
$UmLib = Join-Path $SdkLib "um\x64"
foreach ($required in @(
    (Join-Path $MsvcInclude "yvals.h"),
    (Join-Path $SdkInclude "um\windows.h"),
    (Join-Path $MsvcLib "msvcrt.lib"),
    (Join-Path $UcrtLib "ucrt.lib"),
    (Join-Path $UmLib "windowsapp.lib"),
    (Join-Path $UmLib "d3d11.lib"),
    (Join-Path $UmLib "dxgi.lib"),
    (Join-Path $HostX64Include "host\atomic128-ldst.h.inc"),
    (Join-Path $HostGenericInclude "host\atomic128-cas.h.inc"),
    (Join-Path $HostGenericInclude "host\atomic128-ldst.h.inc")
)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { Fail "Required MSVC/SDK file is missing: $required" }
}

if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path ([IO.Path]::GetTempPath()) ("xemu-uwp-clangcl-core-smoke-" + [guid]::NewGuid().ToString("N"))
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

$SourceRoot = Join-Path $PSScriptRoot ".."
$MainSource = Join-Path $SourceRoot "src\main.cpp"
$ClearSource = Join-Path $RepositoryRoot "hw\xbox\nv2a\pgraph\d3d11\clear.cc"
$ClearCSource = Join-Path $RepositoryRoot "hw\xbox\nv2a\pgraph\clear.c"
foreach ($source in @($MainSource, $ClearSource, $ClearCSource)) {
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { Fail "Required source is missing: $source" }
}

$IncludeArgs = @(
    "/I$BuildDirectory",
    "/I$($TargetHeader.DirectoryName)",
    "/I$RepositoryRoot",
    "/I$(Join-Path $RepositoryRoot 'include')",
    "/I$HostX64Include",
    "/I$HostGenericInclude",
    "/I$(Join-Path $RepositoryRoot 'hw\xbox\nv2a\pgraph\d3d11')",
    "/I$MsvcInclude",
    "/I$UcrtInclude",
    "/I$UmInclude",
    "/I$SharedInclude",
    "/I$CppWinrt"
)
$TargetDefine = '/DCONFIG_TARGET="' + $TargetHeader.Name + '"'
$Defines = @(
    "/DCOMPILING_PER_TARGET",
    $TargetDefine,
    "/DWIN32_LEAN_AND_MEAN",
    "/DWINAPI_FAMILY=WINAPI_FAMILY_APP",
    "/D_WIN32_WINNT=0x0A00"
)
$Common = @(
    "/nologo", "/c", "/std:c++17", "/EHsc", "/W4", "/WX", "/O2",
    "/MD", "/GS", "/Zc:__cplusplus"
) + $Defines + $IncludeArgs
$MainObj = Join-Path $OutputDirectory "main.obj"
$ClearObj = Join-Path $OutputDirectory "clear.obj"
$ClearCObj = Join-Path $OutputDirectory "clear-c.obj"
$Exe = Join-Path $OutputDirectory "xemu-uwp-clangcl-core-smoke.exe"

function Invoke-Checked([string]$Tool, [string[]]$Arguments) {
    & $Tool @Arguments
    if ($LASTEXITCODE -ne 0) { Fail "'$Tool' exited with code $LASTEXITCODE." }
}

Invoke-Checked $ClangCl ($Common + @("/Fo$MainObj", $MainSource))
Invoke-Checked $ClangCl ($Common + @("/Fo$ClearObj", $ClearSource))
$CCommon = @(
    "/nologo", "/c", "/TC", "/W4", "/WX", "/O2", "/MD", "/GS",
    "/DWIN32_LEAN_AND_MEAN", "/DWINAPI_FAMILY=WINAPI_FAMILY_APP",
    "/DCOMPILING_PER_TARGET", $TargetDefine
) + $IncludeArgs
Invoke-Checked $ClangCl ($CCommon + @("/Fo$ClearCObj", $ClearCSource))

$LibArgs = @(
    "/libpath:$MsvcLib", "/libpath:$UcrtLib", "/libpath:$UmLib",
    "/subsystem:windows", "/entry:mainCRTStartup", "/machine:x64",
    "/appcontainer", "/dynamicbase", "/nxcompat", "/out:$Exe",
    $MainObj, $ClearObj, $ClearCObj,
    "windowsapp.lib", "runtimeobject.lib", "d3d11.lib", "dxgi.lib",
    "ole32.lib"
)
Invoke-Checked $LldLink $LibArgs

$ReadObj = Find-Tool "llvm-readobj.exe" @($ClangBin)
$Dumpbin = Find-Tool "dumpbin.exe" @((Join-Path $MsvcRoot.FullName "bin\Hostx64\x64"))
$AuditPath = Join-Path $OutputDirectory "imports.txt"
if ($ReadObj) {
    & $ReadObj --coff-imports $Exe | Tee-Object -FilePath $AuditPath
    if ($LASTEXITCODE -ne 0) { Fail "llvm-readobj import audit failed." }
} elseif ($Dumpbin) {
    & $Dumpbin /nologo /imports $Exe | Tee-Object -FilePath $AuditPath
    if ($LASTEXITCODE -ne 0) { Fail "dumpbin import audit failed." }
} else {
    Fail "Neither llvm-readobj.exe nor dumpbin.exe is available for the required import audit."
}
$Imports = Get-Content -LiteralPath $AuditPath -Raw
if ($Imports -match '(?i)libatomic|libgcc|libstdc\+\+|cygwin|mingw') {
    Fail "Unsupported non-MSVC runtime import detected; see $AuditPath."
}
if ($Imports -notmatch '(?i)ucrtbase|vcruntime|api-ms-win-crt|msvcrt') {
    Fail "The linked executable has no recognizable dynamic MSVC CRT import; see $AuditPath."
}

Write-Host "Built AppContainer smoke executable: $Exe"
Write-Host "Real generated host config: $ConfigHost"
Write-Host "Real generated target config: $($TargetHeader.FullName)"
Write-Host "Import audit: $AuditPath"
Write-Host "Compile/link/import-audit only: direct AppContainer execution is intentionally disabled until an MSIX package identity exists."
