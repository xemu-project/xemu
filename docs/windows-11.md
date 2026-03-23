# Windows 11 (64-bit) build and run guide

This is the fastest path we currently support for building and launching OpenMidway on **Windows 11 x86-64** from a normal PowerShell window.

## Recommended setup

- **OS:** Windows 11 64-bit
- **Toolchain:** MSYS2 `MINGW64`
- **Shell you start from:** Windows PowerShell or PowerShell 7

## 1. Install MSYS2 once

Install MSYS2 from [msys2.org](https://www.msys2.org/) using the default location:

```text
C:\msys64
```

> The helper script below assumes `C:\msys64`. If you installed MSYS2 somewhere else, pass `-MsysRoot` explicitly.

## 2. From PowerShell, install dependencies and build

From the repository root, run:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\windows\openmidway-win11.ps1 all
```

That command will:

1. update MSYS2,
2. install the required MinGW-w64 packages,
3. build OpenMidway, and
4. leave the packaged Windows files in `dist\`.

If MSYS2 updates itself during the first run, close PowerShell, open it again, and rerun the same command once more.

## 3. Launch the built emulator

After a successful build, start OpenMidway with:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\windows\openmidway-win11.ps1 run
```

The packaged executable is also available directly at:

```text
dist\xemu.exe
```

## Useful one-command shortcuts

### Check your MSYS2 + repo setup first

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\windows\openmidway-win11.ps1 doctor
```

This verifies that MSYS2 was found, confirms whether `dist\xemu.exe` already exists, and warns if core MinGW packages are still missing.

### Install/update dependencies only

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\windows\openmidway-win11.ps1 bootstrap
```

### Rebuild without reinstalling packages

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\windows\openmidway-win11.ps1 build
```

### Open an MSYS2 MINGW64 shell at the repo root

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\windows\openmidway-win11.ps1 shell
```

Use this when you want to run `./build.sh`, inspect package state, or debug a failing build without manually navigating into MSYS2 first.

### Preview commands without changing anything

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\windows\openmidway-win11.ps1 all -DryRun
```

## What the script installs

The helper script installs the MSYS2 packages OpenMidway needs most often on Windows 11 x86-64, including:

- GCC / binutils
- Meson / Ninja / pkgconf
- Python
- SDL2
- GTK3
- GLib
- Pixman
- libepoxy
- libslirp
- libssh
- libnfs
- zstd

## Manual fallback

If `run` says `dist\xemu.exe` is missing, that now means the helper could not find a completed package yet; rerun `build` or `doctor` first.

If you prefer to build manually inside MSYS2, the underlying command is still:

```bash
./build.sh
```

Run that from an **MSYS2 MinGW 64-bit** shell opened at the repository root.
