[CmdletBinding()]
param(
    [ValidateSet('bootstrap', 'build', 'run', 'all', 'doctor', 'shell')]
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

function Assert-RepoRoot {
    if (-not (Test-Path (Join-Path $RepoPath 'build.sh'))) {
        throw "OpenMidway build.sh was not found under '$RepoPath'. Run this script from an OpenMidway checkout or pass -RepoPath explicitly."
    }
}

function Get-Msys2CommandOutput {
    param([Parameter(Mandatory = $true)][string]$Script)

    if (-not (Test-Path $bashExe)) {
        throw "MSYS2 bash was not found at '$bashExe'. Install MSYS2 first from https://www.msys2.org/."
    }

    $oldPath = $env:PATH
    $oldMsystem = $env:MSYSTEM
    $oldHere = $env:CHERE_INVOKING

    try {
        $env:MSYSTEM = 'MINGW64'
        $env:CHERE_INVOKING = '1'
        $env:PATH = "$mingwBin;$usrBin;$oldPath"

        $output = & $bashExe -lc $Script 2>&1
        $exitCode = $LASTEXITCODE
        return [pscustomobject]@{
            ExitCode = $exitCode
            Output = @($output)
        }
    } finally {
        $env:PATH = $oldPath
        $env:MSYSTEM = $oldMsystem
        $env:CHERE_INVOKING = $oldHere
    }
}

function Test-Msys2PackageInstalled {
    param([Parameter(Mandatory = $true)][string]$PackageName)

    $result = Get-Msys2CommandOutput -Script "pacman -Q $PackageName"
    return $result.ExitCode -eq 0
}

$repoMsysPath = Convert-ToMsysPath -Path $RepoPath
$packageList = ($packages -join ' ')

function Invoke-Doctor {
    Assert-RepoRoot

    Write-Host '==> OpenMidway Windows 11 helper diagnostics'
    Write-Host ("Repo path:        {0}" -f $RepoPath)
    Write-Host ("MSYS2 root:       {0}" -f $MsysRoot)
    Write-Host ("MSYS2 bash:       {0}" -f $bashExe)
    Write-Host ("Repo path (MSYS): {0}" -f $repoMsysPath)
    Write-Host ("build.sh:         {0}" -f $(if (Test-Path (Join-Path $RepoPath 'build.sh')) { 'found' } else { 'missing' }))
    Write-Host ("dist\\xemu.exe:    {0}" -f $(if (Test-Path (Join-Path $RepoPath 'dist\\xemu.exe')) { 'ready to run' } else { 'not built yet' }))

    $issues = @()

    if (-not (Test-Path $bashExe)) {
        $issues += "MSYS2 bash was not found at '$bashExe'."
    }

    $pacmanOk = $false
    if (-not $issues.Count) {
        $pacmanCheck = Get-Msys2CommandOutput -Script 'command -v pacman'
        $pacmanOk = $pacmanCheck.ExitCode -eq 0
        if (-not $pacmanOk) {
            $issues += 'pacman is unavailable inside the selected MSYS2 installation.'
        }
    }

    if ($pacmanOk) {
        $requiredPackages = @(
            'mingw-w64-x86_64-gcc',
            'mingw-w64-x86_64-meson',
            'mingw-w64-x86_64-ninja',
            'mingw-w64-x86_64-SDL2'
        )
        $missingPackages = @($requiredPackages | Where-Object {
            -not (Test-Msys2PackageInstalled -PackageName $_)
        })

        if ($missingPackages.Count) {
            Write-Warning ("Missing key MSYS2 packages: {0}" -f ($missingPackages -join ', '))
        } else {
            Write-Host 'Key MSYS2 packages: installed'
        }
    }

    if ($issues.Count) {
        foreach ($issue in $issues) {
            Write-Error $issue
        }
        throw 'OpenMidway Windows helper diagnostics failed.'
    }

    Write-Host ''
    Write-Host 'Next commands:'
    Write-Host '  bootstrap  - install/update dependencies'
    Write-Host '  build      - build OpenMidway'
    Write-Host '  run        - launch dist\xemu.exe'
    Write-Host '  shell      - open an MSYS2 MINGW64 shell at the repo root'
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

switch ($Command) {
    'doctor' {
        Invoke-Doctor
    }
    'bootstrap' {
        Assert-RepoRoot
        Invoke-Msys2 'pacman -Syu --noconfirm'
        Invoke-Msys2 "pacman -S --needed --noconfirm $packageList"
    }
    'build' {
        Assert-RepoRoot
        Invoke-Msys2 "cd '$repoMsysPath' && ./build.sh"
    }
    'run' {
        Assert-RepoRoot
        if (-not (Test-Path (Join-Path $RepoPath 'dist\\xemu.exe'))) {
            throw "dist\\xemu.exe was not found under '$RepoPath'. Run the build command first: powershell -ExecutionPolicy Bypass -File .\\scripts\\windows\\openmidway-win11.ps1 build"
        }
        Invoke-Msys2 "cd '$repoMsysPath' && ./dist/xemu.exe"
    }
    'all' {
        Assert-RepoRoot
        Invoke-Msys2 'pacman -Syu --noconfirm'
        Invoke-Msys2 "pacman -S --needed --noconfirm $packageList"
        Invoke-Msys2 "cd '$repoMsysPath' && ./build.sh"
    }
    'shell' {
        Assert-RepoRoot
        Invoke-Msys2 "cd '$repoMsysPath' && exec bash --login -i"
    }
}
