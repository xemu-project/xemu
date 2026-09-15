# xemu Xbox UWP host probe

This is a small x64 UWP app for Xbox One/Series Dev Mode. It emits one JSON object per line to the debugger and to `ApplicationData.Current.LocalFolder\xemu-uwp-host-probe.jsonl`.

The probe covers generated RW→RX code, a 4 GiB virtual reservation, a thread-local value on a worker thread, app-local storage, gamepad enumeration, and a complete D3D11 frame path: a deterministic BGRA pattern is uploaded through xemu's shared `ui/xui/D3D11FrameUploader`, copied to a CoreWindow flip-model swap-chain backbuffer without shaders, GPU-completed with a `D3D11_QUERY_EVENT`, stage-read back before `Present`, and then presented. It also runs xemu's reusable `hw/xbox/nv2a/pgraph/d3d11/D3D11ClearPrimitive` against deterministic offscreen RGBA/BGRA, rectangular color, Z16, and Z24S8 depth/stencil targets, including explicit unsupported mask/float/swizzled cases. After the clear matrix, the checked `D3D11DrawPrimitive` consumes the offline DXBC in `draw_shaders.h`, draws an indexed clockwise triangle into an offscreen BGRA RTV with an explicit rasterizer state, waits for GPU completion, validates inside/outside staging pixels, and copies that same surface to the presenter when compatible. The draw milestone emits `d3d11_draw_after_clear` JSONL telemetry with status, HRESULT, feature level, GPU completion, and observed readback fields kept separate from `ok`.

## Build/package

Open `xemu-uwp-host-probe.sln` (or the `.vcxproj`) in Visual Studio with the Universal Windows Platform workload and SDK 26100. Select `Release | x64`, then **Build > Build Solution**. Package with:

The `/MD` UWP build imports the Microsoft C++ app-container runtime, so the package declares the official x64 `Microsoft.VCLibs.140.00` framework dependency. The dependency is intentionally not checked into this repository. Obtain the signed package from Microsoft's official `microsoft/winget-cli` release dependency archive and verify the archive hash before packaging:

```powershell
$url = "https://github.com/microsoft/winget-cli/releases/download/v1.12.350/DesktopAppInstaller_Dependencies.zip"
$zip = Join-Path $env:TEMP "DesktopAppInstaller_Dependencies.zip"
Invoke-WebRequest $url -OutFile $zip -UseBasicParsing
if ((Get-FileHash $zip -Algorithm SHA256).Hash -ne
    "906CAD3B2BE067D816B20EA4EB1DF541F8A23AC4A4AA9FED70CE675CD918E6A6") {
    throw "Unexpected Microsoft dependency archive hash"
}
$depRoot = Join-Path $env:TEMP "DesktopAppInstaller_Dependencies"
Expand-Archive $zip -DestinationPath $depRoot -Force
$vclibs = Join-Path $depRoot "x64\Microsoft.VCLibs.140.00_14.0.33519.0_x64.appx"
.\scripts\package.ps1 -Configuration Release -Platform x64 -VCLibsPackagePath $vclibs
```

`package.ps1` inspects the dependency's actual `AppxManifest.xml` and requires `Name="Microsoft.VCLibs.140.00"`, `ProcessorArchitecture="x64"`, Microsoft’s publisher, and version `14.0.33519.0` or newer. It also verifies the Microsoft AppX signature with `signtool`, then copies the dependency beside the generated MSIX under `AppPackages`. This package is the base UWP runtime, not the separately named `UWPDesktop` framework.

The script invokes MSBuild and `MakeAppx.exe`; it stages the manifest, executable, generated valid PNG logo, and copies the verified framework package beside the MSIX. It deliberately does not create or contain a certificate/private key. To sign with an existing PFX, pass `-CertificatePath C:\path\probe.pfx` and optionally `-CertificatePassword ...`; no certificate is generated. In Xbox Dev Mode, enable Device Portal, install the generated `.msix`/`.appx` using the Apps page, launch the probe, and retrieve the JSONL file from the app's local state.

For the SDK/MSVC-only path, `scripts/build-direct.ps1` resolves the repository root and compiles the authoritative `ui/xui/d3d11-present.cc`, `ui/xui/d3d11-corewindow-present.cc`, `hw/xbox/nv2a/pgraph/d3d11/clear.cc`, and `hw/xbox/nv2a/pgraph/d3d11/draw.cc` together with the probe under `/ZW`, `/APPCONTAINER`, and the normal dynamic UWP CRT linkage. The draw shaders are consumed from the generated offline `draw_shaders.h`; `d3dcompiler.lib` and runtime `D3DCompile` are not used. It creates deterministic probe-only `config-host.h`, `probe-config-target.h`, and `config-poison.h` files under `obj\<Platform>\<Configuration>-direct\probe-generated`; no generated configuration header is written into the repository. The native `hw/xbox/nv2a/pgraph/clear.c` helper is compiled separately as C without `/ZW` and linked into the probe. The runtime dependency is supplied at the package step via `-VCLibsPackagePath`; no static CRT substitution is made. A copied standalone probe is still supported: copy both presenter source/header pairs, `clear.cc`/`clear.h`, `draw.cc`/`draw.h`/`draw_shaders.h`, and `pgraph/clear.c`/`clear.h` into the probe's `src` directory (or pass `-RepositoryRoot` to point at a checkout); the script falls back to `src` when the repository-root sources are unavailable.

The `.vcxproj` is the IDE path and uses the same project-local generated-header directory under `$(IntDir)probe-generated`; its `GenerateProbeConfig` target creates those three minimal headers before compilation. Both paths add the repository root and `include` directory explicitly, so a clean checkout does not depend on an unrelated previously generated `config-host.h`.

The manifest uses a development publisher placeholder. A Dev Mode certificate matching that publisher must be supplied locally when packaging/installing; never commit it.

## Device Portal helper

`scripts/deploy-devmode.ps1` validates the package manifest publisher, SHA-256, declared
framework dependency, dependency identity/version, and signatures before talking to an Xbox. It is a dry run unless `-Deploy` is supplied;
it never creates certificates or writes credentials. The helper uses the documented
Device Portal endpoints `/api/app/packagemanager/package`,
`/api/app/packagemanager/state`, `/api/taskmanager/app`,
`/api/app/packagemanager/packages`, and `/api/filesystem/apps/file`.
Installation is sent as `multipart/form-data`; the package filename is supplied
with the `package` query parameter and dependency/certificate filenames with repeated
`dependency` or `certificate` parameters. If `-DependencyPath` is omitted, the helper
resolves each declared dependency from the package directory (the package script puts
the official VCLibs AppX there); use `-DependencyPath` to override that explicitly. The helper polls package-manager state and reports
explicit installation errors or timeouts instead of claiming success.

Inspect a signed package without contacting an Xbox (optionally pin its expected hash):

```powershell
.\scripts\deploy-devmode.ps1 -PackagePath .\AppPackages\probe.msix `
  -CertificatePath C:\keys\probe.cer -ExpectedSha256 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
```

Upload/install, optionally launch, and fetch the probe JSONL after enabling Xbox
Developer Mode and Device Portal:

```powershell
.\scripts\deploy-devmode.ps1 -PackagePath .\AppPackages\probe.msix `
  -DevicePortalUri https://192.168.0.50:11443 -Deploy -Launch -Telemetry `
  -SkipCertificateCheck
```

Without `-Credential`, the script asks for Device Portal credentials using a secure
PowerShell prompt. `-SkipCertificateCheck` is required only for an untrusted
Device Portal HTTPS certificate and is intentionally opt-in. The package must be
signed already; this helper does not sign or generate certificates. Telemetry is
best effort because the app's package-local path and Device Portal file permissions
can vary; it enumerates the `InstalledPackages` response wrapper and downloads
`LocalState\xemu-uwp-host-probe.jsonl` through `/api/filesystem/apps/file` with
`knownfolderid=LocalAppData`. Use
`-TelemetryOutputPath` to choose the local output file. Launch uses the required
`package` and `appid` query parameters. `-InstallTimeoutSeconds` controls the
package-manager polling timeout (default: 120 seconds).

## Validation limits

This repository's Linux/macOS environment cannot compile UWP C++ or execute an Xbox package. The source and project can still be inspected with `git diff --check`; package/install validation must happen on the Windows VM and Xbox Dev Mode.

> Agent declaration: implementation assisted by OpenAI Codex GPT-5 / Luna subagent.
