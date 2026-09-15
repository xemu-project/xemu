param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [string]$SdkVersion = "10.0.26100.0",
    [string]$RepositoryRoot
)

$ErrorActionPreference = "Stop"
if ($Platform -ne "x64") { throw "This direct probe build currently supports x64 only." }
$ProbeRoot = Split-Path -Parent $PSScriptRoot
if (-not $RepositoryRoot) {
    # scripts -> uwp-host-probe -> xbox -> tests -> repository root.
    $RepositoryRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
}
$RepositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot -ErrorAction Stop).Path
$Out = Join-Path $ProbeRoot "bin\$Platform\$Configuration"
$Int = Join-Path $ProbeRoot "obj\$Platform\$Configuration-direct"
$GeneratedInclude = Join-Path $Int "probe-generated"
New-Item -ItemType Directory -Force -Path $Out | Out-Null
New-Item -ItemType Directory -Force -Path $Int | Out-Null
New-Item -ItemType Directory -Force -Path $GeneratedInclude | Out-Null
@'
#ifndef CONFIG_HOST_H
#define CONFIG_HOST_H
#define CONFIG_WIN32 1
#define CONFIG_TARGET "probe-config-target.h"
#endif
'@ | Set-Content -LiteralPath (Join-Path $GeneratedInclude "config-host.h") -Encoding ascii
@'
#ifndef PROBE_CONFIG_TARGET_H
#define PROBE_CONFIG_TARGET_H
#define TARGET_BIG_ENDIAN 0
#define TARGET_LONG_BITS 64
#endif
'@ | Set-Content -LiteralPath (Join-Path $GeneratedInclude "probe-config-target.h") -Encoding ascii
@'
#ifndef CONFIG_POISON_H
#define CONFIG_POISON_H
#endif
'@ | Set-Content -LiteralPath (Join-Path $GeneratedInclude "config-poison.h") -Encoding ascii

function Find-MsvcRoot([string]$VsRoot) {
    if (-not (Test-Path -LiteralPath $VsRoot -PathType Container)) { return $null }
    $MsvcTools = Join-Path $VsRoot "VC\Tools\MSVC"
    $MsvcRoot = Get-ChildItem -LiteralPath $MsvcTools -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending | Select-Object -First 1
    if ($MsvcRoot -and (Test-Path -LiteralPath (Join-Path $MsvcRoot.FullName "bin\Hostx64\x64\cl.exe") -PathType Leaf)) {
        return $MsvcRoot
    }
    return $null
}

function Find-VisualStudioInstallation {
    # vswhere is installed with current Visual Studio installers. Query all
    # products so Community, Professional, Enterprise, and Build Tools are
    # treated equally, then retain the first installation with VC tools.
    $VsWhereCandidates = @(
        (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"),
        (Join-Path ${env:ProgramFiles} "Microsoft Visual Studio\Installer\vswhere.exe")
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) }
    foreach ($VsWhere in $VsWhereCandidates) {
        $Installations = & $VsWhere -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format value -property installationPath 2>$null
        foreach ($Installation in $Installations) {
            $Msvc = Find-MsvcRoot ([string]$Installation)
            if ($Msvc) { return @{ Root = [string]$Installation; Msvc = $Msvc } }
        }
    }

    # Keep the historical VS18 Community location working even when vswhere
    # is absent (and support the corresponding VS 2022/2026 and Build Tools
    # layouts used by standalone or pre-release installations).
    $ProgramFilesRoots = @(${env:ProgramFiles}, ${env:ProgramFiles(x86)}) | Where-Object { $_ } | Select-Object -Unique
    foreach ($ProgramFilesRoot in $ProgramFilesRoots) {
        foreach ($Version in @("18", "17", "2026", "2022")) {
            foreach ($Edition in @("Community", "Professional", "Enterprise", "BuildTools")) {
                $Root = Join-Path $ProgramFilesRoot "Microsoft Visual Studio\$Version\$Edition"
                $Msvc = Find-MsvcRoot $Root
                if ($Msvc) { return @{ Root = $Root; Msvc = $Msvc } }
            }
        }
    }
    throw "Visual Studio with VC tools (VS 2022/2026 or Build Tools) was not found. Install the Desktop C++ workload or make vswhere.exe available."
}

$VisualStudio = Find-VisualStudioInstallation
$VsRoot = $VisualStudio.Root
$MsvcRoot = $VisualStudio.Msvc
$SdkRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10"
$Sdk = Join-Path $SdkRoot "Include\$SdkVersion"
$SdkLib = Join-Path $SdkRoot "Lib\$SdkVersion"
$Cl = Join-Path $MsvcRoot.FullName "bin\Hostx64\x64\cl.exe"
$Link = Join-Path $MsvcRoot.FullName "bin\Hostx64\x64\link.exe"
$Lib = Join-Path $MsvcRoot.FullName "bin\Hostx64\x64\lib.exe"
foreach ($p in @($Cl,$Link,$Sdk,$SdkLib)) { if (-not (Test-Path $p)) { throw "Required toolchain path not found: $p" } }
$MsvcInc = $MsvcRoot.FullName + "\include"
$UcrtInc = Join-Path $Sdk "ucrt"
$UmInc = Join-Path $Sdk "um"
$SharedInc = Join-Path $Sdk "shared"
$WinrtInc = Join-Path $Sdk "winrt"
# Keep the dynamic UWP CRT linkage. package.ps1 validates and stages the
# matching signed Microsoft.VCLibs framework; do not silently switch to /MT.
$MsvcLib = $MsvcRoot.FullName + "\lib\x64"
$UcrtLib = Join-Path $SdkLib "ucrt\x64"
$UmLib = Join-Path $SdkLib "um\x64"
$Winmd = Join-Path $SdkRoot "UnionMetadata\$SdkVersion"
$Refs = Join-Path $SdkRoot "References\$SdkVersion"
$PlatformWinmd = Join-Path $MsvcRoot.FullName "lib\x86\store\references"
$Obj = Join-Path $Int "main.obj"
$UploaderObj = Join-Path $Int "d3d11-present.obj"
$CoreWindowObj = Join-Path $Int "d3d11-corewindow-present.obj"
$ClearObj = Join-Path $Int "clear.obj"
$DrawObj = Join-Path $Int "draw.obj"
$ClearCObj = Join-Path $Int "pgraph-clear.obj"
$Exe = Join-Path $Out "xemu-uwp-host-probe.exe"
$UploaderSource = Join-Path $RepositoryRoot "ui\xui\d3d11-present.cc"
$CoreWindowSource = Join-Path $RepositoryRoot "ui\xui\d3d11-corewindow-present.cc"
$ClearSource = Join-Path $RepositoryRoot "hw\xbox\nv2a\pgraph\d3d11\clear.cc"
$DrawSource = Join-Path $RepositoryRoot "hw\xbox\nv2a\pgraph\d3d11\draw.cc"
$ClearCSource = Join-Path $RepositoryRoot "hw\xbox\nv2a\pgraph\clear.c"
$RepositoryInclude = $RepositoryRoot
$RepositoryGeneratedInclude = Join-Path $RepositoryRoot "include"
$SharedInclude = Join-Path $RepositoryRoot "ui\xui"
$ClearInclude = Join-Path $RepositoryRoot "hw\xbox\nv2a\pgraph\d3d11"
if (-not (Test-Path -LiteralPath $UploaderSource -PathType Leaf) -or
    -not (Test-Path -LiteralPath $CoreWindowSource -PathType Leaf) -or
    -not (Test-Path -LiteralPath $ClearSource -PathType Leaf) -or
    -not (Test-Path -LiteralPath $DrawSource -PathType Leaf) -or
    -not (Test-Path -LiteralPath $ClearCSource -PathType Leaf)) {
    # A copied probe remains buildable when both shared presenter sources and
    # headers are copied next to src/main.cpp. The repository-root sources are
    # preferred for normal builds.
    $UploaderSource = Join-Path $ProbeRoot "src\d3d11-present.cc"
    $CoreWindowSource = Join-Path $ProbeRoot "src\d3d11-corewindow-present.cc"
    $ClearSource = Join-Path $ProbeRoot "src\clear.cc"
    $DrawSource = Join-Path $ProbeRoot "src\draw.cc"
    $ClearCSource = Join-Path $ProbeRoot "src\clear.c"
    $SharedInclude = Join-Path $ProbeRoot "src"
    $ClearInclude = Join-Path $ProbeRoot "src"
}
if (-not (Test-Path -LiteralPath $UploaderSource -PathType Leaf) -or
    -not (Test-Path -LiteralPath $CoreWindowSource -PathType Leaf) -or
    -not (Test-Path -LiteralPath $ClearSource -PathType Leaf) -or
    -not (Test-Path -LiteralPath $DrawSource -PathType Leaf) -or
    -not (Test-Path -LiteralPath $ClearCSource -PathType Leaf) -or
    -not (Test-Path -LiteralPath (Join-Path $SharedInclude "d3d11-present.h") -PathType Leaf) -or
    -not (Test-Path -LiteralPath (Join-Path $SharedInclude "d3d11-corewindow-present.h") -PathType Leaf) -or
    -not (Test-Path -LiteralPath (Join-Path $ClearInclude "clear.h") -PathType Leaf) -or
    -not (Test-Path -LiteralPath (Join-Path $ClearInclude "draw.h") -PathType Leaf) -or
    -not (Test-Path -LiteralPath (Join-Path $ClearInclude "draw_shaders.h") -PathType Leaf)) {
    throw "D3D11 presenter/clear/draw sources and headers not found. Use the repository root or copy d3d11-present.cc/.h, d3d11-corewindow-present.cc/.h, clear.cc/.h/clear.c, draw.cc/.h, and draw_shaders.h into src for a standalone probe build."
}
$Includes = @("/I$GeneratedInclude","/I$MsvcInc","/I$UcrtInc","/I$UmInc","/I$SharedInc","/I$WinrtInc","/I$RepositoryInclude","/I$RepositoryGeneratedInclude","/I$SharedInclude","/I$ClearInclude")
$LibPaths = @("/LIBPATH:$MsvcLib","/LIBPATH:$UcrtLib","/LIBPATH:$UmLib")
$Defines = @('/DUNICODE','/D_UNICODE','/DWINAPI_FAMILY=WINAPI_FAMILY_APP','/DWIN32_LEAN_AND_MEAN','/DCOMPILING_PER_TARGET')
& $Cl /nologo /c /std:c++17 /ZW /EHsc /W4 /WX /O2 /GS /MD /guard:cf "/AI$PlatformWinmd" "/AI$Winmd" "/AI$Refs" @Defines @Includes "/Fo$Obj" (Join-Path $ProbeRoot "src\main.cpp")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $Cl /nologo /c /std:c++17 /ZW /EHsc /W4 /WX /O2 /GS /MD /guard:cf "/AI$PlatformWinmd" "/AI$Winmd" "/AI$Refs" @Defines @Includes "/Fo$UploaderObj" $UploaderSource
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $Cl /nologo /c /std:c++17 /ZW /EHsc /W4 /WX /O2 /GS /MD /guard:cf "/AI$PlatformWinmd" "/AI$Winmd" "/AI$Refs" @Defines @Includes "/Fo$CoreWindowObj" $CoreWindowSource
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $Cl /nologo /c /std:c++17 /ZW /EHsc /W4 /WX /O2 /GS /MD /guard:cf "/AI$PlatformWinmd" "/AI$Winmd" "/AI$Refs" @Defines @Includes "/Fo$ClearObj" $ClearSource
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $Cl /nologo /c /std:c++17 /ZW /EHsc /W4 /WX /O2 /GS /MD /guard:cf "/AI$PlatformWinmd" "/AI$Winmd" "/AI$Refs" @Defines @Includes "/Fo$DrawObj" $DrawSource
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $Cl /nologo /c /TC /W4 /WX /O2 /GS /MD /guard:cf "/AI$PlatformWinmd" "/AI$Winmd" "/AI$Refs" @Defines @Includes "/Fo$ClearCObj" $ClearCSource
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $Link /nologo /subsystem:windows /entry:mainCRTStartup /machine:x64 /appcontainer /DYNAMICBASE /NXCOMPAT @LibPaths "/OUT:$Exe" $Obj $UploaderObj $CoreWindowObj $ClearObj $DrawObj $ClearCObj windowsapp.lib d3d11.lib dxgi.lib
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "Built $Exe"
