# Getting Started

This guide is for people who want to get OpenMidway running quickly without digging through the full QEMU/xemu documentation stack.

## Before You Start

You will need:

- An original Xbox MCPX boot ROM / BIOS dump
- An Xbox hard disk image or a game image you are legally allowed to use
- A controller or keyboard mapping that works on your platform
- A build of OpenMidway for your OS, or a local source build

> OpenMidway does not ship copyrighted Xbox firmware, hard-drive contents, or game data.

## Fast Path: First Successful Boot

1. Build OpenMidway or download a build artifact. On Windows 11 x86-64, the quickest source-build path is `powershell -ExecutionPolicy Bypass -File .\scripts\windows\openmidway-win11.ps1 all`.
2. Put your BIOS/boot ROM and disk/game files somewhere easy to find.
3. Launch OpenMidway.
4. Point the emulator at your required files when prompted.
5. Confirm video, audio, and controller input all work before changing advanced settings.
6. Save that working configuration before experimenting with networking or renderer tweaks.

If your goal is online play, get a single-player boot working first. That separates basic setup problems from System Link networking issues.

## Recommended First-Run Checklist

Use this order to avoid the most common setup mistakes:

### 1. Verify core files

Make sure you know the exact location of your:

- BIOS / boot ROM
- HDD image or game image
- Save-data directory

Keeping all emulator-related files under one clearly named folder makes future troubleshooting much easier.

### 2. Start with default graphics settings

If a game boots, avoid changing renderer-related settings immediately. Establish a known-good baseline first, then tune performance one change at a time.

### 3. Confirm input early

Before spending time on networking or performance tweaks:

- Check that a controller is detected
- Confirm sticks and triggers map correctly
- Verify you can navigate the dashboard or game menus

### 4. Test audio and video on a known-good title

Use a title that is already considered stable for your local setup. If possible, avoid making a first-boot judgment using an already-problematic game.

### 5. Only then set up networking

System Link features are easiest to debug after the emulator already boots games correctly by itself.

## Common Setup Mistakes

### "It launches, but I don't know what to configure first"

Start with firmware, storage/game media, display output, and controller input. Leave advanced networking and performance tuning for later.

### "My game is slow, so I changed a bunch of settings"

Change one setting at a time and keep notes. Large batches of tweaks make it difficult to tell whether the improvement came from the renderer, audio, timing, or networking changes.

### "System Link doesn't work"

Treat this as a second-stage setup task. First confirm:

- The game boots reliably
- Input works
- Audio/video are stable
- The title actually supports System Link

Then move on to the architecture notes in [docs/networking.md](networking.md).

## Best Next Documents

- [README.md](../README.md) for the project overview and build command
- [docs/windows-11.md](windows-11.md) for the quickest Windows 11 source-build path
- [docs/networking.md](networking.md) for System Link and LAN tunneling details
- [docs/performance.md](performance.md) for optimization strategy and bottlenecks
- [docs/compatibility-triage.md](compatibility-triage.md) for reporting whether a title is playable, functional, or broken

## Contributor Note

If you improve usability, setup flow, or first-run documentation, update this guide so new users benefit from the same shortcut you discovered.
