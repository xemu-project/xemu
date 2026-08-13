# xemu

xemu is an original Xbox emulator, built as a fork of QEMU. The official
project site — with prebuilt downloads, FAQ, compatibility list, and general
troubleshooting — lives at [https://xemu.app](https://xemu.app). This README
covers building and running xemu from source.

> If you just want to *play games*, you almost certainly want the official
> prebuilt binary from [xemu.app/download](https://xemu.app/download/). Build
> from source only if you are developing on xemu or need an unreleased change.

---

## What you need before you start

xemu emulates real Xbox hardware, and it needs the same files a real Xbox
does. **These files are not distributed with xemu** — you have to provide them.

| File            | What it is                                        |
| --------------- | ------------------------------------------------- |
| MCPX boot ROM   | 512-byte boot ROM from the MCPX southbridge       |
| Flash BIOS      | 256 KB / 1 MB BIOS image (a retail or hacked one) |
| EEPROM          | 256-byte per-console EEPROM dump                  |
| HDD image       | A formatted Xbox hard drive image (`.qcow2`)      |

The legal way to obtain these is to dump them from a console you own. See the
[xemu quickstart guide](https://xemu.app/docs/getting-started/) for a walk-through.

Games are loaded as `.iso` files (or as an XBE for homebrew). You are
responsible for owning any game you dump and run.

---

## Building from source

### 1. Install build dependencies

**macOS** (Xcode Command Line Tools + Homebrew):

```bash
xcode-select --install
brew install pkg-config python3 dylibbundler meson ninja libepoxy sdl2 \
             glib pixman
```

Optional but recommended on macOS: install the [Vulkan SDK](https://vulkan.lunarg.com/)
so the Vulkan renderer (which runs over MoltenVK) can be built and bundled.

**Linux (Debian / Ubuntu):**

```bash
sudo apt install build-essential git python3 python3-pip ninja-build \
                 libepoxy-dev libsdl2-dev libglib2.0-dev libpixman-1-dev \
                 libsamplerate0-dev libpcap-dev libslirp-dev \
                 libvulkan-dev
```

**Windows:** use [MSYS2](https://www.msys2.org/) and install the MinGW-w64
toolchain plus the same package set (`mingw-w64-x86_64-*` versions of the
dependencies above). Build inside an MSYS2 MinGW64 shell.

### 2. Clone the repo

xemu uses git submodules. Clone recursively:

```bash
git clone --recursive https://github.com/xemu-project/xemu.git
cd xemu
```

If you already cloned without `--recursive`:

```bash
git submodule update --init --recursive
```

### 3. Build

**Always build via `./build.sh`** — it sets the correct configure flags for
xemu, restricts the QEMU target list to i386, and runs the platform-specific
packaging step. Do not call `configure` / `make` directly.

```bash
./build.sh                # release build
./build.sh --debug        # debug build (asserts, tracing, XEMU_DEBUG_BUILD=1)
./build.sh -j8            # override parallel job count
```

Output:

- Binary: `build/qemu-system-i386` (Linux/macOS) or
  `build/qemu-system-i386w.exe` (Windows).
- Packaged app: `dist/xemu.app` (macOS), `dist/xemu` (Linux), `dist/xemu.exe`
  (Windows).
- Full build log: `build.log`.

**Incremental rebuilds** (much faster than re-running `build.sh`, which
reconfigures from scratch):

```bash
ninja -C build qemu-system-i386
# or
make -C build qemu-system-i386
```

---

## Running xemu

### macOS

```bash
open dist/xemu.app
```

### Linux

```bash
./dist/xemu
```

### Windows

Double-click `dist\xemu.exe`, or run it from a terminal.

### First-time setup

1. On first launch, open `Machine -> Settings` (or `xemu -> Settings` on macOS).
2. Point xemu at your MCPX boot ROM, flash BIOS, EEPROM, and HDD image.
3. Set your DVD drive to a game `.iso` under `Machine -> Load Disc`.
4. `Machine -> Start` to boot.

Settings are saved to a `xemu.toml` config file. Its location differs per OS
— xemu shows the path under `Help -> About` (or check `~/.local/share/xemu/`
on Linux, `~/Library/Application Support/xemu/` on macOS,
`%APPDATA%\xemu\` on Windows).

---

## Troubleshooting a build

- **`SDK >= 14.0 not found` (macOS Apple Silicon):** update Xcode / Command
  Line Tools. Apple Silicon builds target macOS 14.0+; Intel builds target
  12.7.5+.
- **`Vulkan SDK not found` warning (macOS):** harmless — the OpenGL renderer
  still works. Install the Vulkan SDK and re-run `build.sh` to enable the
  Vulkan backend.
- **Missing submodule / meson errors:** run
  `git submodule update --init --recursive` and try again.
- **Something else is broken:** check `build.log` for the actual error, then
  ask on the xemu Discord (link on [xemu.app](https://xemu.app)) or open an
  issue.

---

## Where to go next

- Project site & docs: [https://xemu.app](https://xemu.app)
- Compatibility list: [https://xemu.app/compatibility](https://xemu.app/compatibility/)
- Contributor notes: see `CLAUDE.md` in this repo for an architecture overview
  (Xbox hardware layout, NV2A GPU pipeline, renderer selection, config schema).
