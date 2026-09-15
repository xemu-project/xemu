[CmdletBinding()]
param(
    [string]$RepositoryRoot
)

$ErrorActionPreference = "Stop"

function Fail([string]$Message) {
    throw "xemu Xbox Windows test configure failed: $Message"
}

if (-not $RepositoryRoot) {
    $RepositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}
$RepositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot -ErrorAction Stop).Path
$buildDirectory = Join-Path $RepositoryRoot "build-xbox-uwp-tests"
$repoPrefix = $RepositoryRoot.TrimEnd('\') + '\'
if (-not $buildDirectory.StartsWith($repoPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    Fail "test build directory escaped the repository root."
}
if (Test-Path -LiteralPath $buildDirectory) {
    Remove-Item -LiteralPath $buildDirectory -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $buildDirectory | Out-Null

$configure = Join-Path $RepositoryRoot "configure"
if (-not (Test-Path -LiteralPath $configure -PathType Leaf)) { Fail "configure script is missing." }
$bashCandidates = @(
    (Join-Path ${env:ProgramFiles} "Git\bin\bash.exe"),
    (Join-Path ${env:ProgramFiles(x86)} "Git\bin\bash.exe"),
    (Join-Path ${env:ProgramFiles} "MSYS2\usr\bin\bash.exe"),
    (Join-Path ${env:ProgramFiles(x86)} "MSYS2\usr\bin\bash.exe")
) | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) }
$bash = $null
foreach ($candidate in $bashCandidates) {
    $bash = Get-Item -LiteralPath $candidate
    break
}
if (-not $bash) { Fail "Git Bash or MSYS2 bash was not found in the standard installation paths; PATH-only bash is not accepted." }
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) { Fail "vswhere.exe is missing." }
$vsRoot = & $vswhere -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -latest -property installationPath 2>$null
if (-not $vsRoot) { Fail "Visual Studio C++ tools workload is missing." }
$vsDevCmd = Join-Path ([string]$vsRoot) "Common7\Tools\VsDevCmd.bat"
if (-not (Test-Path -LiteralPath $vsDevCmd -PathType Leaf)) { Fail "VsDevCmd.bat is missing: $vsDevCmd" }

$llvmCandidates = [System.Collections.Generic.List[string]]::new()
$pathClang = Get-Command clang.exe -ErrorAction SilentlyContinue
if ($pathClang) { [void]$llvmCandidates.Add($pathClang.Source) }
$llvmCandidates.Add((Join-Path ${env:ProgramFiles} "LLVM\bin\clang.exe"))
$vsLlvm = @(& $vswhere -products * -requires Microsoft.VisualStudio.Component.VC.Llvm.Clang -latest -find "VC\Tools\Llvm\x64\bin\clang.exe" 2>$null)
foreach ($candidate in $vsLlvm) { [void]$llvmCandidates.Add([string]$candidate) }
$clang = $null
foreach ($candidate in $llvmCandidates) {
    if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        $clang = (Resolve-Path -LiteralPath $candidate).Path
        break
    }
}
if (-not $clang) { Fail "an installed LLVM clang.exe is required for the authentic Windows Meson build; clang-cl alone is reserved for the AppContainer smoke." }
$llvmBin = Split-Path -Parent $clang
$clangxx = Join-Path $llvmBin "clang++.exe"
$lld = Join-Path $llvmBin "ld.lld.exe"
foreach ($tool in @($clangxx, $lld)) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) { Fail "LLVM sibling tool is missing beside clang.exe: $tool" }
}
$llvmAr = Join-Path $llvmBin "llvm-ar.exe"
$llvmNm = Join-Path $llvmBin "llvm-nm.exe"
$llvmRanlib = Join-Path $llvmBin "llvm-ranlib.exe"
foreach ($tool in @($llvmAr, $llvmNm, $llvmRanlib)) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) { Fail "LLVM binutils tool is missing beside clang.exe: $tool" }
}
$llvmDlltool = Join-Path $llvmBin "llvm-dlltool.exe"
$llvmRc = Join-Path $llvmBin "llvm-rc.exe"
$rc = if (Test-Path -LiteralPath $llvmRc -PathType Leaf) { $llvmRc } else { $null }
$dlltool = if (Test-Path -LiteralPath $llvmDlltool -PathType Leaf) { $llvmDlltool } else { $null }
$mesonCommand = Get-Command meson.exe -ErrorAction SilentlyContinue
if (-not $mesonCommand -or -not (Test-Path -LiteralPath $mesonCommand.Source -PathType Leaf)) {
    Fail "The pinned Meson executable is not available on PATH."
}

function Convert-WindowsPathToMsys([string]$Path) {
    $full = [IO.Path]::GetFullPath($Path)
    $match = [regex]::Match($full, '^([A-Za-z]):\\(.*)$')
    if (-not $match.Success) { Fail "Cannot convert Windows path to an MSYS path: $full" }
    return "/$($match.Groups[1].Value.ToLowerInvariant())/$($match.Groups[2].Value.Replace('\', '/'))"
}

function Quote-Bash([string]$Value) {
    return "'" + $Value.Replace("'", "'\''") + "'"
}

# Configure is run from a clean, fixed in-tree build directory. It therefore
# generates the real config-host.h/config-host.mak/Meson coredata that the
# clang-cl smoke and the test gate inspect; no headers are synthesized.
$cmdFile = Join-Path $env:RUNNER_TEMP "xemu-configure-xbox-tests.cmd"
$bashScript = Join-Path $env:RUNNER_TEMP "xemu-configure-xbox-tests.sh"
$buildForCmd = $buildDirectory.Replace('/', '\')
$repoForCmd = $RepositoryRoot.Replace('/', '\')
$vsForCmd = $vsDevCmd.Replace('/', '\')
$bashForCmd = $bash.FullName.Replace('/', '\')
$bashScriptMsys = Convert-WindowsPathToMsys $bashScript
$repoMsys = Convert-WindowsPathToMsys $RepositoryRoot
$buildMsys = Convert-WindowsPathToMsys $buildDirectory
$configureMsys = Convert-WindowsPathToMsys $configure
$llvmBinMsys = Convert-WindowsPathToMsys $llvmBin
$clangMsys = Convert-WindowsPathToMsys $clang
$clangxxMsys = Convert-WindowsPathToMsys $clangxx
$lldMsys = Convert-WindowsPathToMsys $lld
$llvmArMsys = Convert-WindowsPathToMsys $llvmAr
$llvmNmMsys = Convert-WindowsPathToMsys $llvmNm
$llvmRanlibMsys = Convert-WindowsPathToMsys $llvmRanlib
$mesonMsys = Convert-WindowsPathToMsys $mesonCommand.Source
$dlltoolMsys = if ($dlltool) { Convert-WindowsPathToMsys $dlltool } else { $null }
$rcMsys = if ($rc) { Convert-WindowsPathToMsys $rc } else { $null }
$bashLines = @(
    '#!/usr/bin/env bash',
    'set -euo pipefail',
    "export PATH=$(Quote-Bash $llvmBinMsys):`$PATH",
    "export CC=$(Quote-Bash $clangMsys)",
    "export CXX=$(Quote-Bash $clangxxMsys)",
    "export LD=$(Quote-Bash $lldMsys)",
    "export AR=$(Quote-Bash $llvmArMsys)",
    "export NM=$(Quote-Bash $llvmNmMsys)",
    "export RANLIB=$(Quote-Bash $llvmRanlibMsys)"
)
if ($dlltoolMsys) { $bashLines += "export DLLTOOL=$(Quote-Bash $dlltoolMsys)" }
if ($rcMsys) {
    $bashLines += "export WINDRES=$(Quote-Bash $rcMsys)"
    # Keep RC as an explicit compatibility alias for Meson versions that
    # inspect it, while QEMU configure's canonical variable is WINDRES.
    $bashLines += "export RC=$(Quote-Bash $rcMsys)"
}
$bashLines += @(
    "cd $(Quote-Bash $repoMsys)",
    # Repository-authoritative bootstrap: meson subprojects download must run
    # before the configure --disable-download gate below.
    "$(Quote-Bash $mesonMsys) subprojects download",
    'test -f subprojects/keycodemapdb/README',
    "cd $(Quote-Bash $buildMsys)",
    "$(Quote-Bash $configureMsys) --cc=$(Quote-Bash $clangMsys) --cxx=$(Quote-Bash $clangxxMsys) --extra-ldflags=$(Quote-Bash ("-fuse-ld=$lldMsys")) --target-list=x86_64-softmmu --disable-werror --disable-docs --disable-tools --disable-download"
)
$bashLines -join "`n" | Set-Content -LiteralPath $bashScript -Encoding ASCII
@"
@echo off
call "$vsForCmd" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
call "$bashForCmd" -lc "$bashScriptMsys"
exit /b %errorlevel%
"@ | Set-Content -LiteralPath $cmdFile -Encoding ASCII
& cmd.exe /d /s /c $cmdFile
if ($LASTEXITCODE -ne 0) {
    Fail "the authentic configure/Meson setup failed with exit code $LASTEXITCODE; no fallback or fake build directory is accepted."
}

foreach ($required in @(
    (Join-Path $buildDirectory "meson-private\coredata.dat"),
    (Join-Path $buildDirectory "config-host.h"),
    (Join-Path $buildDirectory "config-host.mak"))) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { Fail "authentic configure did not create '$required'." }
}
$configMak = Get-Content -LiteralPath (Join-Path $buildDirectory "config-host.mak") -Raw
if ($configMak -notmatch '(?m)^TARGET_DIRS=.*x86_64-softmmu') {
    Fail "generated config-host.mak does not contain the exact x86_64-softmmu target."
}
$envValues = @(
    "XEMU_VSDEVCMD=$vsDevCmd",
    "XEMU_BASH=$($bash.FullName)",
    "XEMU_MESON=$($mesonCommand.Source)",
    "XEMU_LLVM_BIN=$llvmBin",
    "CC=$clang",
    "CXX=$clangxx",
    "LD=$lld",
    "AR=$llvmAr",
    "NM=$llvmNm",
    "RANLIB=$llvmRanlib"
)
if ($dlltool) { $envValues += "DLLTOOL=$dlltool" }
if ($rc) {
    $envValues += "WINDRES=$rc"
    # Compatibility alias; configure/Meson consumes WINDRES.
    $envValues += "RC=$rc"
}
if ($env:GITHUB_ENV) { $envValues | Add-Content -LiteralPath $env:GITHUB_ENV -Encoding UTF8 }
Write-Output $buildDirectory
