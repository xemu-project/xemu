[CmdletBinding()]
param(
    [ValidateSet('bootstrap', 'build', 'run', 'all')]
    [string]$Command = 'all',

    [string]$MsysRoot = 'C:\msys64',

    [string]$RepoPath = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,

    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'

$bashExe = Join-Path $MsysRoot 'usr\bin\bash.exe'
$mingwBin = Join-Path $MsysRoot 'mingw64\bin'
$usrBin = Join-Path $MsysRoot 'usr\bin'

$packages = @(
    'base-devel',
    'git',
    'mingw-w64-x86_64-SDL2',
    'mingw-w64-x86_64-epoxy',
    'mingw-w64-x86_64-gcc',
    'mingw-w64-x86_64-glib2',
    'mingw-w64-x86_64-gtk3',
    'mingw-w64-x86_64-libnfs',
    'mingw-w64-x86_64-libslirp',
    'mingw-w64-x86_64-libssh',
    'mingw-w64-x86_64-meson',
    'mingw-w64-x86_64-ninja',
    'mingw-w64-x86_64-pixman',
    'mingw-w64-x86_64-pkgconf',
    'mingw-w64-x86_64-python',
    'mingw-w64-x86_64-zstd'
)

function Convert-ToMsysPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    $normalized = $Path.Replace('\\', '/')
    if ($normalized -match '^([A-Za-z]):/(.*)$') {
        return '/' + $matches[1].ToLowerInvariant() + '/' + $matches[2]
    }

    return $normalized
}

function Invoke-Msys2 {
    param([Parameter(Mandatory = $true)][string]$Script)

    if (-not (Test-Path $bashExe)) {
        throw "MSYS2 bash was not found at '$bashExe'. Install MSYS2 first from https://www.msys2.org/."
    }

    Write-Host "==> $Script"
    if ($DryRun) {
        return
    }

    $oldPath = $env:PATH
    $oldMsystem = $env:MSYSTEM
    $oldHere = $env:CHERE_INVOKING

    try {
        $env:MSYSTEM = 'MINGW64'
        $env:CHERE_INVOKING = '1'
        $env:PATH = "$mingwBin;$usrBin;$oldPath"

        & $bashExe -lc $Script
        if ($LASTEXITCODE -ne 0) {
            throw "MSYS2 command failed with exit code $LASTEXITCODE"
        }
    } finally {
        $env:PATH = $oldPath
        $env:MSYSTEM = $oldMsystem
        $env:CHERE_INVOKING = $oldHere
    }
}

$repoMsysPath = Convert-ToMsysPath -Path $RepoPath
$packageList = ($packages -join ' ')

switch ($Command) {
    'bootstrap' {
        Invoke-Msys2 'pacman -Syu --noconfirm'
        Invoke-Msys2 "pacman -S --needed --noconfirm $packageList"
    }
    'build' {
        Invoke-Msys2 "cd '$repoMsysPath' && ./build.sh"
    }
    'run' {
        Invoke-Msys2 "cd '$repoMsysPath' && test -f dist/xemu.exe && ./dist/xemu.exe"
    }
    'all' {
        Invoke-Msys2 'pacman -Syu --noconfirm'
        Invoke-Msys2 "pacman -S --needed --noconfirm $packageList"
        Invoke-Msys2 "cd '$repoMsysPath' && ./build.sh"
    }
}
