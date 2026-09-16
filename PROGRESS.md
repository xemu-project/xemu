# xemu Xbox Series X|S UWP / Direct3D 11 – Übergabe

Stand: 2026-09-16

Dieses Dokument ist die maßgebliche Übergabe für neue Agents. Es beschreibt
den tatsächlichen Zustand des Worktrees und die noch fehlenden Gates für einen
vollständigen xemu-Build als x64-UWP-App im Xbox-Series-X|S-Developer-Mode.
Ein Diagnose-Host-Probe-Paket ist **nicht** der xemu-Build und darf nicht als
Xbox-fähiges Release bezeichnet werden.

## 1. Ziel und harte Grenzen

Das Ziel ist ein installierbares und startbares x64-AppX/MSIX für xemu auf
Xbox Series X|S im Developer Mode:

1. vollständiger xemu-Core inklusive NV2A/PGRAPH,
2. produktiver Direct3D-11-PGRAPH-Renderer,
3. SDL-freier UWP-Host mit CoreWindow und D3D11-Swapchain,
4. Windows-Verifikation in Parallels, inklusive Build, Test und Paketprüfung,
5. reproduzierbares Paketieren und Signieren,
6. anschließende Installation/Start auf Xbox Dev Mode durch den Benutzer.

Scope ist ausschließlich Xbox Dev Mode, x64 und die Windows/Parallels-
Verifikation. Linux-, macOS-, ARM64- und Store-/Retail-Ziele sind für dieses
Ziel nicht erforderlich. Der Agent kann den Xbox-Hardwaretest nicht ersetzen;
ein echter Device-Portal-Installations- und Laufnachweis auf der Konsole ist
das abschließende Hardware-Gate des Benutzers.

## 2. Aktueller Git-Zustand (VERIFIED)

Aus dem Worktree am Stand dieses Dokuments:

| Feld | Status / Wert |
|---|---|
| Branch | `feat/xbox-uwp-d3d11-foundation` |
| `HEAD` | `21c0419e962c120ea2f50101ba98b7fe98989897` |
| `origin` | `origin/feat/xbox-uwp-d3d11-foundation` zeigt auf denselben Commit |
| letzter Commit | `nv2a: Isolate D3D11 from QEMU C headers` |
| Worktree | **DIRTY**; es gibt modified und untracked Dateien |
| Commit/Push dieser dirty Änderungen | **nicht erfolgt** |
| Release/Workflow-Dispatch nach diesen dirty Änderungen | **nicht erfolgt** |

Aktuell als modified gemeldet:

```text
config_spec.yml
hw/xbox/nv2a/framebuffer.h
hw/xbox/nv2a/pgraph/d3d11/meson.build
hw/xbox/nv2a/pgraph/d3d11/surface.cc
hw/xbox/nv2a/pgraph/d3d11/surface.h
hw/xbox/nv2a/pgraph/pgraph.c
hw/xbox/nv2a/pgraph/pgraph.h
meson.build
meson_options.txt
tests/unit/d3d11-surface-test.cc
tests/unit/test-d3d11-bridge-abi-cxx.cc
tests/unit/test-d3d11-bridge-abi.c
ui/xui/main-menu.cc
ui/xui/menubar.cc
ui/xui/meson.build
```

Aktuell untracked:

```text
hw/xbox/nv2a/pgraph/d3d11/renderer.c
hw/xbox/nv2a/pgraph/d3d11/renderer.cc
hw/xbox/nv2a/pgraph/d3d11/renderer.h
ui/xui/uwp/
```

Neue Agents müssen diese Änderungen zuerst mit `git diff` und einem
Windows-Build prüfen. Sie dürfen nicht stillschweigend davon ausgehen, dass
der dirty Stand bereits Teil des gepushten Branches ist.

## 3. Was auf dem letzten gepushten Stand vorhanden ist

Die folgenden Punkte sind am Commit `21c0419e96` vorhanden. Das bedeutet
„implementiert/verifiziert“ nur im jeweils genannten Umfang, nicht „voller
Xbox-UWP-Port“.

| Bereich | Status | Tatsächlicher Umfang |
|---|---|---|
| Renderer-neutrale Framebuffer-Basis | VERIFIED | Grund-ABI und Lease-/Validierungsbasis aus `hw/xbox/nv2a/framebuffer.h`/PGRAPH sind vorhanden; der produktive D3D11-Export ist damit nicht bewiesen. |
| Windows-D3D11-Presentation-Abstraktion | VERIFIED | `ui/xui/d3d11-present.*` und `ui/xui/d3d11-corewindow-present.*` enthalten Upload-/CoreWindow-Swapchain-Grundlagen. |
| D3D11-CLEAR | VERIFIED | Clear-Primitive, Adapter, Executor und unterstützte Farb-/Depth-Fälle einschließlich deterministischer Tests sind vorhanden. |
| D3D11-Surface-Cache | VERIFIED | Owner-thread-, Upload-/Download-, Retire-/Download-Event- und Readback-Grundlagen sind vorhanden; noch kein vollständiger PGRAPH-Produktionspfad. |
| D3D11-DRAW | VERIFIED | Ein begrenzter Triangle-/Vertex-/State-Pfad und CPU-Vertex-Übersetzung sind vorhanden und getestet. Das ist keine vollständige NV2A-Shader-/Texture-Abdeckung. |
| PSH | VERIFIED | Offline-Referenz-/Interpreter-Grundlagen und WARP/CPU-Tests vorhanden. Das beweist nicht die Einbindung aller NV2A-PSH-Register und Texture-Stages in den Renderer. |
| VSH | VERIFIED | CPU-Transform-/Passthrough-Grundlagen und Tests vorhanden; vollständige produktive PGRAPH-Anbindung und alle NV2A-Fälle sind nicht belegt. |
| C/C++-Bridge | VERIFIED | `d3d11_bridge.c/.h` isolieren die Windows-C++-Einheiten von QEMU-GNU-C-Headerproblemen; ABI- und Adaptertests vorhanden. |
| Windows-C++-Kompatibilität | VERIFIED | Die auf dem letzten grünen Windows-Lauf benötigten Header-/Typ-Anpassungen sind gepusht. |
| UWP-Host-Probe | VERIFIED | `tests/xbox/uwp/host-probe` ist ein separater x64-Diagnosehost mit CoreWindow/D3D11, AppContainer-Funktionen, Storage-/Gamepad-/Readback-Proben. |
| vollständiger xemu-UWP-Host | MISSING | Es gibt noch kein `ui/xui/uwp/xemu-uwp.vcxproj` und kein produktives App-Entry-Point/Manifest für xemu. |

## 4. Belastbare bisherige Nachweise

### 4.1 GitHub-Nachweis (VERIFIED, aber nur für den gepushten Stand)

Der letzte bekannte vollständig erfolgreiche Windows-fokussierte Lauf ist:

`https://github.com/anthonyhfm/xxemu/actions/runs/35029784026`

Dort liefen am gepushten Commit Windows x86_64 Debug/Release, die Windows-
PDB-Jobs, das Source-Paket und der Xbox-UWP-Host-Probe-Diagnosejob durch.
Linux, macOS und Unit-Test-Jobs waren auf dem Feature-Branch übersprungen.

Dieser Lauf beweist **nicht**:

- keinen vollständigen xemu-UWP-Link,
- kein `ui/xui/uwp/xemu-uwp.vcxproj`,
- kein produktives xemu-MSIX,
- keine D3D11-PGRAPH-Framebufferschnittstelle,
- keinen Xbox-Install/Start.

Er bezieht sich außerdem nicht auf die derzeit uncommitted Dateien. Nach
weiteren Änderungen muss ein Windows-Build erneut lokal auf Parallels geprüft
werden, bevor überhaupt ein gezielter CI-Lauf sinnvoll ist.

### 4.2 Lokaler Parallels-Nachweis (VERIFIED, Diagnoseumfang)

Außerhalb des Repositories liegt:

```text
/Users/anthony/Desktop/parallels-xemu-setup/README.md
/Users/anthony/Desktop/parallels-xemu-setup/Invoke-XemuWindowsChecks.ps1
```

Die Windows-VM verfügt über:

- Visual Studio Community 2026 18.7 und x64-MSVC,
- Windows SDK `10.0.26100.0`, einschließlich `makeappx.exe` und `signtool.exe`,
- `clang-cl` aus dem Android-NDK,
- user-scoped Meson/Ninja.

Die lokalen isolierten Bridge-/D3D11-Clear-/Draw-Tests liefen durch. Ein
unsigned x64-Diagnose-MSIX wurde gebaut und mit MakeAppx inspiziert:

```text
Identity: xemu.UwpHostProbe
Architecture: x64
SHA-256: 240B626A968C9FA5FC28FD60DB92C91D45A35938CDB283E2323DF286148424C2
```

Das Paket ist ausdrücklich **kein** vollständiges xemu-Paket, wurde nicht als
Xbox-Release installiert und ist nicht als UWP-Produktionsnachweis zu werten.

### 4.3 Aktueller dirty Stand (UNVERIFIED)

Für die aktuellen Änderungen wurde noch kein vollständiger authentischer
Windows-UWP-Link und kein vollständiger xemu-Start durchgeführt. `git
diff --check` war sauber; das ist nur ein Format-/Whitespace-Nachweis. Die
uncommitted Renderer-/UWP-Dateien sind daher als **UNVERIFIED** zu behandeln,
auch wenn einzelne C++-Dateien syntaktisch plausibel aussehen.

## 5. Exakte fehlende technische Gates für den Xbox-Build

Die folgenden Punkte müssen erledigt und jeweils mit einem konkreten Nachweis
abgehakt werden. Ein „Skeleton“, ein Host-Probe-Artifact oder ein grüner
Cross-Compile zählt nicht als Erfüllung.

### Gate A – produktive D3D11-PGRAPH-Registrierung und Callback-Vertrag

Status: **UNVERIFIED / INCOMPLETE**.

Die uncommitted `hw/xbox/nv2a/pgraph/d3d11/renderer.c/.cc/.h` enthalten eine
Registrierungsshell und die Tabelle wird über den Constructor registriert. Der
aktuelle Code initialisiert Clear/Draw/Flush und lässt den Framebuffer-Getter
explizit `false` zurückgeben. Mehrere Pfade sind nur No-Op oder Platzhalter:

- `get_framebuffer_surface` liefert derzeit immer `false`;
- `get_report` schreibt nur einen Nullwert, Visibility Queries fehlen;
- `image_blit` ist nicht implementiert;
- `draw_begin` und `process_pending_reports` sind leer;
- `pre_savevm_wait` und `pre_shutdown_wait` warten nicht auf einen echten
  GPU-/Download-/Shutdown-Zustand;
- `surface_flush` ist nicht als vollständiger D3D11-Surface-Vertrag
  implementiert;
- GPU-Properties, primitive winding und alle PGRAPH-Zustände sind nicht
  vollständig aus NV2A abgebildet.

Für dieses Gate muss ein Agent die komplette `PGRAPHRenderer`-Callback-
Semantik mit dem Verhalten der GL-/Vulkan-Renderer vergleichen und für D3D11
implementieren oder bewusst begründet ablehnen, wobei ein Spiel mit
unsupported state nicht als stiller Erfolg behandelt werden darf. Der Pfad
muss über `g_config.display.renderer = D3D11` tatsächlich ausgewählt,
initialisiert, benutzt und finalisiert werden können. Dafür braucht es einen
authentischen xemu-Lauf bzw. einen gleichwertigen Windows-Test, nicht nur ein
isoliertes Executor-Testprogramm.

### Gate B – echte Framebuffer-Erzeugung, Export, Readback und Handoff

Status: **MISSING**.

`NV2AFramebufferSurface` wurde dirty um einen D3D11-Typ, ein opakes Resource-
Feld, Metadaten und eine Borrowed-Lease erweitert. Das ist nur das ABI. Der
produzierende D3D11-Renderer füllt keinen gültigen Descriptor und bietet keinen
`ID3D11Texture2D`-Handoff an. Der UWP-Host erwartet dagegen im Render-Callback
eine geliehene Texture vom selben Device und kopiert sie in die CoreWindow-
Backbuffer-Texture.

Erforderlich:

1. eine aktuelle Color-Render-Target-Texture im PGRAPH-Surface-Cache,
2. format-/pitch-/dimensionstreue NV2A-zu-D3D11-Konvertierung,
3. ein stabiler borrowed `ID3D11Texture2D`-Pointer mit dokumentierter Lease,
4. Device-/Context-Gleichheit zwischen Renderer und CoreWindow-Presenter,
5. sichere `nv2a_get_framebuffer_surface()`-/`nv2a_release...()`-Sequenz,
6. Readback/Download in guest VRAM vor CPU-sichtbaren Zugriffen,
7. Resize-/Scale-/Device-Lost-Verhalten,
8. kein Release/Retain der geliehenen COM-Texture nach Lease-Ende,
9. ein Windows-Test, der einen echten PGRAPH-Frame über den Handoff bis
   `Present()` und zurück bis zum Readback prüft.

Der vorhandene CPU-Readback-Code im Surface-Resource ist nicht automatisch
ein UWP-Framebuffer-Handoff. Beides muss im produktiven Renderer verbunden
und mit Lifetime-/Thread-Tests abgesichert werden.

### Gate C – vollständige Texture-, PSH- und VSH-Pipeline

Status: **MISSING**.

Die vorhandenen Primitives decken nur einen begrenzten, explizit unterstützten
Subset ab. Für xemu-Produktionsbetrieb fehlen mindestens:

- NV2A-Texture-Unit-State aus den PGRAPH-Registern bis zu D3D11-SRV/Sampler,
- Upload/Invalidierung aller verwendeten Texturformate und Layouts,
- Swizzled/linear/packed/compressed/mipmapped/cubemap-Fälle, soweit der
  jeweilige xemu-PGRAPH-Pfad sie verwendet,
- Texture-Cache-Invalidierung und VRAM-Dirty-Regeln,
- vollständiges PSH-Debug-/Reference- oder übersetztes Shader-Verhalten für
  die tatsächlich unterstützten NV2A-Programme,
- PSH-Register, Combiner, Fog, Alpha-Test, depth/stencil und Blend-Parität,
- VSH-Programmfetch, Constant-/Input-Register, Output-Register und alle
  Programm-Modi im produktiven Draw-Pfad,
- Bindung der VSH-/PSH-Ergebnisse an echte D3D11-Shader/Input-Layouts,
- korrekte Vertex-/Index-/Primitive-Modi über den bisher getesteten
  Triangle-List-Subset hinaus.

Der frühere Versuch eines universellen giant `/Od`-DXBC-VSH wurde verworfen,
weil WARP bei der Ausführung abstürzte. Nicht wieder als „fertig“ einbauen.
Der belastbare Weg ist ein kleiner, verifizierter Shader-/CPU-Fallback mit
expliziten Unsupported-Statuswerten und anschließendem Ausbau; die Definition
of Done verlangt jedoch die tatsächliche von xemu benötigte Abdeckung, nicht
nur einen Testdreieck-Pfad.

### Gate D – Render-/Flush-/Shutdown-Threadmodell

Status: **UNVERIFIED / INCOMPLETE**.

Der D3D11-Context und die Surface-Resources sind owner-thread gebunden. Der
CoreWindow-Host rendert und präsentiert auf seinem UI-/CoreWindow-Thread. Es
ist noch nicht bewiesen, dass beide auf demselben Thread mit demselben
Immediate Context laufen. Der aktuelle Shell-Code ruft Flush in mehreren
Callbacks auf, hat aber keinen vollständigen Marshal-/Queue-Vertrag.

Zu implementieren und zu testen:

- ein eindeutiger Owner für `ID3D11DeviceContext`, Surface-Cache und GPU-
  Ressourcen,
- sichere Übergabe PGRAPH-Thread ↔ CoreWindow-Thread oder ein gemeinsamer
  Owner-Thread,
- Flush-, sync_pending-, flush_pending- und event-Reihenfolge ohne Deadlock,
- Download-Event-Drain und VRAM-Dirty-Clear genau einmal pro Generation,
- Resize/Scale erst nach abgeschlossener GPU-Arbeit,
- `pre_savevm_*` und `pre_shutdown_*` mit echtem Warten und Fehlerpfad,
- Device-Lost-/Timeout-/Exception-Pfad,
- idempotentes Shutdown einschließlich Presenter-, Context- und Texture-
  Freigabe,
- Test unter Hardware-D3D11 und WARP, inklusive mehrerer Frames und Resize.

### Gate E – SDL-freier CoreWindow-Host an xemu anbinden

Status: **MISSING**.

`ui/xui/uwp/corewindow-host.cc` implementiert einen generischen
`IFrameworkView`-/`CoreApplication::Run`-Host mit D3D11-Swapchain und
Callbacks. Es gibt aber noch keinen UWP-Anwendungsentrypoint, der xemu
initialisiert, den Emulator-Thread startet, die PGRAPH-Texture liefert und
alle Lebenszyklusereignisse an xemu weitergibt. Der Host ist derzeit eine
Bibliothek, keine startbare xemu-App.

Noch erforderlich:

- produktives UWP `main`/EntryPoint bzw. C++/CX-App-Objekt,
- vollständige xemu-Core-/Machine-/Config-Initialisierung ohne SDL,
- Emulator-Thread-/PGRAPH-Thread-Start und sauberer Exit,
- CoreWindow resize, visibility, suspend, resume, closed und activation,
- Presenter-Backbuffer-Handoff an den produktiven D3D11-PGRAPH-Renderer,
- keine HWND-, WGL-, Desktop-SDL- oder nicht erlaubten Win32-Abhängigkeiten,
- ein authentischer AppContainer-Start auf Windows.

### Gate F – UWP Storage, Input und Networking

Status: **MISSING**.

`ui/xui/uwp/storage-lifecycle.*` ist nur ein runtime-neutraler C-ABI-Vertrag
mit Contract-Test. Die eigentliche Anpassung an
`ApplicationData::Current->LocalFolder`,
`Package::Current->InstalledLocation`, Settings und Save-State ist nicht in
den xemu-Core integriert. Gamepad-Aufzählung existiert im Host-Probe-Programm,
nicht als xemu-Input-Backend. Ein UWP-fähiger Netzwerkpfad und die dafür
benötigten Manifest-Capabilities sind ebenfalls nicht integriert.

Erforderlich:

- Pfad-/Save-State-/Shader-Cache-Zuordnung auf LocalFolder/TemporaryFolder,
- read-only InstalledLocation für ROM-/Ressourcen-Zugriff,
- UWP Input/Gamepad/Keyboard-orientierte Abbildung auf xemu input-manager,
- Suspend/Resume-Save und Wiederaufnahme,
- erlaubter Netzwerk-Backendpfad einschließlich `internetClient`/weiterer
  minimaler Capability, falls xemu-Netzwerk sie benötigt,
- Test unter AppContainer-Rechten, nicht nur als Desktop-Prozess.

### Gate G – vollständige Meson-/Link-Closure für `xbox_uwp`

Status: **MISSING**.

`meson_options.txt` enthält dirty die Option `xbox_uwp`; `ui/xui/uwp/meson.build`
deklariert eine statische Host-Library und Contract-Test. Es gibt aber kein
Executable-Ziel, das `xemu_uwp_host_dep` mit dem vollständigen xemu-Core
verlinkt. Der normale `xemu`-Desktop-Target bleibt SDL-basiert. Die Option
allein erzeugt daher kein Xbox-Paket.

Erforderlich:

- Windows-only UWP-Executable in Meson oder ein sauber dokumentierter
  MSBuild-Linkpfad zum vollständigen xemu-Core,
- alle für `x86_64-softmmu` notwendigen Core-/NV2A-/UI-/D3D11-Objekte,
- `xemu_uwp_host_dep` tatsächlich in diesem Executable verwenden,
- `/ZW`, `/APPCONTAINER`, `/MD` und kompatible Compiler-/Linker-Flags,
- Ausschluss aller SDL-/OpenGL-/WGL-/Desktop-only-Quellen aus dem UWP-Target,
- Windows SDK libraries (`windowsapp`, `d3d11`, `dxgi`) und VCLibs-Vertrag,
- keine versteckten Abhängigkeiten von einem zuvor erzeugten `config-host.h`,
- Clean-checkout Configure + Build + Link in Parallels.

Die vorhandene `scripts/ci/configure-xbox-uwp-tests.ps1` konfiguriert nur den
authentischen Windows-Testbuild (`x86_64-softmmu`) für die Smoke-/Unit-Gates.
Das ist noch nicht die UWP-App-Link-Closure.

### Gate H – x64-AppContainer-Manifest und Capabilities

Status: **MISSING**.

Der Validator erwartet absichtlich das noch fehlende
`ui/xui/uwp/xemu-uwp.vcxproj` und darunter genau ein
`Package.appxmanifest`. Aktuell existiert nur das Manifest/Projekt unter
`tests/xbox/uwp-host-probe/`; dieses darf nicht wiederverwendet werden.

Das echte Ziel braucht mindestens:

- stabile Package Identity, Name, Version und Publisher (kein
  `xemu.UwpHostProbe`, kein `CN=xemu-development`, kein Placeholder),
- `ProcessorArchitecture="x64"`,
- genau eine `Windows.Xbox`-TargetDeviceFamily, optional zusätzlich
  `Windows.Universal`,
- parsebare `MinVersion`/`MaxVersionTested`,
- explizites `.exe`-Executable und nichtleeren EntryPoint/AppId,
- offizielle x64-Abhängigkeit `Microsoft.VCLibs.140.00` mit Microsoft-
  Publisher und mindestens Version `14.0.33519.0`,
- **nicht** `Microsoft.VCLibs.140.00.UWPDesktop`,
- nur die minimal benötigten Capabilities, voraussichtlich mindestens
  `internetClient` falls Netzwerk benötigt wird,
- Logos/Assets, Resource-/Runtime-Deklarationen und AppService-/Input-
  Deklarationen nur, wenn sie tatsächlich gebraucht werden.

`scripts/ci/validate-xbox-uwp.ps1` ist der verbindliche strukturelle Gate;
solange Projekt und Manifest fehlen, muss er fehlschlagen.

### Gate I – MakeAppx, VCLibs, SignTool und Dev-Mode-Paket

Status: **MISSING** für xemu; **VERIFIED** nur für den separaten Probe-
Mechanismus.

Nach einem echten Build müssen `scripts/ci/build-xbox-uwp.ps1` und
`scripts/ci/sign-xbox-uwp.ps1` für das xemu-Ziel laufen:

- genau ein unsigned x64-MSIX/AppX aus dem vollständigen Target,
- `MakeAppx validate` erfolgreich,
- echte PDB-Symbole vorhanden,
- offizielle passende VCLibs-AppX gestaged und geprüft,
- PFX mit Publisher-Match **oder** explizit kurzlebiges Dev-Test-Zertifikat,
- `signtool` SHA-256-Signatur und `signtool verify /pa /all` erfolgreich,
- Zertifikat/Privatkey niemals in Git oder ungeschützt als Artifact,
- SHA256SUMS, Metadata und SBOM des echten Pakets,
- Paket plus VCLibs plus öffentliches `.cer` für Dev-Mode-Sideload.

Die Beispiel-URL und Hash-Prüfung für die Microsoft-Abhängigkeit sind in
`tests/xbox/uwp-host-probe/README.md` und den CI-Skripten festgeschrieben.

### Gate J – Parallels-Installation und Windows-Ausführung

Status: **MISSING** für den vollständigen xemu-Build.

Nach Gaten A–I muss in Parallels nachgewiesen werden:

- authentischer Clean-build des vollständigen xemu-UWP-Targets,
- CPU-/D3D11-/WARP-Testmatrix erfolgreich,
- `MakeAppx`/SignTool-Validierung erfolgreich,
- Installation des signierten xemu-Pakets unter Windows/AppContainer,
- Start und mindestens ein echter Emulator-Frame,
- LocalFolder-/Input-/Suspend-/Resume-/Shutdown-Pfade,
- Log/Exit ohne fehlende DLL, VCLibs oder Desktop-API.

Der vorhandene Host-Probe-Start reicht dafür nicht.

### Gate K – Xbox Device Portal (Benutzer-Hardware-Gate)

Status: **MISSING / vom Benutzer auszuführen**.

Mit aktiviertem Xbox Developer Mode und Device Portal muss der Benutzer das
signierte Paket samt Microsoft-VCLibs installieren, starten und bestätigen:

- Installation ohne Package-Manager-Fehler,
- xemu-Prozess startet und bleibt aktiv,
- D3D11-Framebuffer wird präsentiert,
- ein Xbox-Spiel/XBE wird bis zum geprüften Zielbild ausgeführt,
- Suspend/Resume/Exit funktionieren,
- relevante Logs/Telemetry werden gesichert.

`tests/xbox/uwp-host-probe/scripts/deploy-devmode.ps1` kann später als
validierter Upload-/Installationshelfer dienen. Es signiert nicht und erzeugt
keine Credentials. Der Hardwaretest selbst kann nicht aus macOS/Parallels-
Build-Verifikation abgeleitet werden.

## 6. Bekannte Blocker und ihre Ursachen

| Blocker | Status | Ursache / Konsequenz |
|---|---|---|
| Full-target-Projekt fehlt | MISSING | `ui/xui/uwp/xemu-uwp.vcxproj` ist nicht vorhanden; `validate-xbox-uwp.ps1` muss deshalb hart fehlschlagen. |
| Full-target-Manifest fehlt | MISSING | `ui/xui/uwp/Package.appxmanifest` ist nicht vorhanden; Probe-Manifest ist kein Ersatz. |
| D3D11-Framebuffer-Handoff | MISSING | Der dirty D3D11-Getter liefert absichtlich `false`; bestehendes GL-only ABI war zuvor unzureichend. |
| Vollständige PGRAPH-Shader/Texture-Parität | MISSING | Vorhandene Clear/Draw/CPU-VSH/PSH-Stücke sind ein begrenzter Subset-/Testpfad. |
| Thread-/Flush-Vertrag | UNVERIFIED | D3D11 Immediate Context ist owner-thread gebunden; CoreWindow-Callback-Thread und PGRAPH-Thread sind noch nicht als ein sicherer Owner-/Marshal-Modell verifiziert. |
| Visual-Studio-UWP-Toolset in Parallels | MISSING | `Microsoft.VisualStudio.ComponentGroup.UWP.VC` ist nicht installiert. Installation benötigt Administratorrechte. |
| Authentischer Meson-UWP-Link | MISSING | Es fehlt die UWP-Executable/Link-Closure; die Test-Konfiguration ist nicht das Produktionsziel. |
| Full xemu MSIX | MISSING | Bisher nur unsigned `xemu.UwpHostProbe`-Diagnosepaket. |
| Xbox Hardware | MISSING | Kein Agent-Hardwaretest; muss nach Build/Signierung vom Benutzer am Device Portal ausgeführt werden. |

Keine dieser Ursachen darf durch ein Probe-Artefakt, einen Cross-Compile oder
einen grünen Windows-Desktop-Build als erledigt markiert werden.

## 7. Parallels: lokale Kommandos und Rechte

### 7.1 Bestehende Diagnosechecks (VERIFIED)

In der Windows-VM aus PowerShell 5.1:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
& C:\Users\anthony\parallels-xemu-setup\Invoke-XemuWindowsChecks.ps1
```

Das führt nur die isolierten ABI-/Clear-/Draw-Tests und die Prüfung des
unsigned Host-Probe-MSIX aus. Es installiert keine App und kontaktiert keine
Xbox.

### 7.2 Fehlendes UWP-Toolset (ADMIN REQUIRED)

Die Installation darf nicht stillschweigend oder über Credential-Umgehungen
erfolgen. Nach expliziter Benutzerfreigabe interaktiv als Administrator in
Parallels:

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vs_installer.exe" modify `
  --installPath "C:\Program Files\Microsoft Visual Studio\18\Community" `
  --add Microsoft.VisualStudio.ComponentGroup.UWP.VC `
  --includeRecommended --passive --norestart
```

Das braucht Elevation, Internet/Installerzugriff und kann reboot-/installer-
abhängig sein. Ohne diese Komponente darf kein MSBuild-UWP-Erfolg behauptet
werden. Windows SDK 10.0.26100.0, MakeAppx und SignTool sind bereits vorhanden.

### 7.3 Full-target-Build, sobald die fehlenden Dateien existieren

Aus dem Repository-Root in PowerShell:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
& .\scripts\ci\validate-xbox-uwp.ps1 `
  -RepositoryRoot (Get-Location).Path
& .\scripts\ci\build-xbox-uwp.ps1 `
  -RepositoryRoot (Get-Location).Path `
  -Configuration Release `
  -Platform x64 `
  -OutputDirectory ((Get-Location).Path + '\xbox-uwp-output')
```

Der Buildscript verweigert andere Architekturen, erwartet das echte
`ui\xui\uwp\xemu-uwp.vcxproj`, findet MSBuild/SDK, baut das vollständige
Target, validiert genau ein MSIX/AppX und verlangt PDB-Symbole.

Für den bestehenden Probe (nur Diagnose):

```powershell
& .\tests\xbox\uwp-host-probe\scripts\build-direct.ps1 `
  -RepositoryRoot (Get-Location).Path `
  -Configuration Release `
  -Platform x64
```

Probe-Paketierung benötigt die geprüfte x64-VCLibs-AppX und erzeugt weiterhin
kein xemu-Release.

### 7.4 Authentischer Testbuild

`scripts/ci/configure-xbox-uwp-tests.ps1` benötigt zusätzlich:

- Git Bash oder MSYS2 an einem Standardpfad,
- vollständiges LLVM (`clang.exe`, `clang++.exe`, `ld.lld.exe`, `llvm-ar`,
  `llvm-nm`, `llvm-ranlib`), nicht nur `clang-cl`,
- VS `VsDevCmd.bat`, Meson und Ninja.

Es erzeugt den sauberen `build-xbox-uwp-tests`-Testbuild und keinen
Produktions-UWP-App-Link. Die anschließende Testmatrix umfasst aktuell:

```text
test-vsh-cpu-transform.exe
test-psh-interpreter.exe
test-psh-interpreter-warp.exe
test-d3d11-present.exe
test-d3d11-corewindow-present.exe
test-d3d11-clear.exe
test-d3d11-clear-adapter.exe
test-d3d11-clear-executor.exe
test-d3d11-draw.exe
test-d3d11-draw-executor.exe
test-d3d11-surface.exe
test-d3d11-state-adapter.exe
test-d3d11-vertex-adapter.exe
```

Alle 13 müssen in einem authentischen Windows-Testlauf bestehen; ein einzelner
Probe-Dreieck-Test genügt nicht.

### 7.5 Signieren und Device Portal (später, nicht jetzt)

Signieren erst nach einem vollständigen, validierten xemu-Paket:

```powershell
& .\scripts\ci\sign-xbox-uwp.ps1 `
  -PackagePath .\xbox-uwp-output\unsigned\xemu-xbox-Release-x64.msix `
  -Mode pfx_required `
  -PfxPath C:\secure\xemu-devmode.pfx `
  -OutputDirectory .\xbox-uwp-output\signing `
  -VclibsPackagePath C:\secure\Microsoft.VCLibs.140.00_x64.appx
```

PFX-Passwort nicht in Git, Shell-History oder öffentliche Actions-Logs
schreiben. Für Xbox Dev Mode kann bewusst ein lokales Testzertifikat genutzt
werden; Publisher im Manifest und Zertifikat müssen exakt übereinstimmen.

Der vorhandene Device-Portal-Helfer ist zunächst dry-run:

```powershell
& .\tests\xbox\uwp-host-probe\scripts\deploy-devmode.ps1 `
  -PackagePath C:\path\to\signed\xemu.msix `
  -CertificatePath C:\path\to\public.cer `
  -ExpectedSha256 <SHA256>
```

Deployment erst nach Benutzerfreigabe und mit Xbox-URL/Credentials:

```powershell
& .\tests\xbox\uwp-host-probe\scripts\deploy-devmode.ps1 `
  -PackagePath C:\path\to\signed\xemu.msix `
  -DevicePortalUri https://<xbox-ip>:11443 `
  -Deploy -Launch -Telemetry -SkipCertificateCheck
```

## 8. CI- und GitHub-Minuten-Regeln

Der Push-CI-Caller ist absichtlich Windows-fokussiert:

- `.github/workflows/ci.yml` ruft `build.yml` mit `windows_only: true` und
  `x64_only: true` auf;
- Linux, macOS und allgemeine Unit-Test-Jobs werden auf diesem Pfad
  übersprungen;
- Source-Paket, Windows x86_64 Debug/Release/PDB und der Xbox-Host-Probe-
  Diagnosejob können trotzdem Runner-Minuten verbrauchen;
- der Host-Probe-Job ist ausdrücklich `diagnostic only` und kein Release-Input;
- `.github/workflows/build-xbox-uwp.yml` ist manuell triggerbar, aber der
  `preflight`-Validator überspringt den Full-target-Build solange Projekt und
  Manifest fehlen;
- die Workflow-Optionen `create_release`, `release_tag`, `draft_release` und
  `prerelease` dürfen bis zum vollständigen DoD nicht aktiviert/benutzt werden.

Zum Sparen von Minuten:

1. zuerst `git diff --check`, lokale Generator-/ABI-/D3D11-Tests und Parallels;
2. dann den echten Windows-UWP-Link lokal reparieren;
3. erst wenn `validate-xbox-uwp.ps1` erfolgreich ist, einen manuellen
   `build-xbox-uwp.yml`-Lauf starten;
4. zunächst `Release`, `x64`, `create_release=false`, kein Debug-Lauf;
5. keine parallelen Dispatches und keine Release-Erzeugung während der
   Entwicklung;
6. nach einem Fehler den konkreten Log lokal reproduzieren, statt blind neue
   Runs zu starten;
7. keine Linux/macOS-Matrix wieder aktivieren, solange dieses Ziel nur Windows/
   Xbox verlangt.

## 9. Empfohlene Reihenfolge der nächsten Arbeiten

1. **Dirty Stand sichern und reviewen (kein Push ohne Review).** `git diff`
   auf Renderer-/Framebuffer-/UWP-Dateien lesen; Build-/ABI-Brüche beheben.
2. **D3D11 Surface- und Framebuffer-Vertrag fertigstellen.** Einen echten
   Color-Surface-Resource-Export inklusive Lease, Device-Gleichheit und
   Readback/Handoff implementieren; Getter darf erst dann `true` liefern.
3. **PGRAPH-Callback-Tabelle vervollständigen.** Init/finalize, pending,
   flush, report, blit, save/shutdown, scale und GPU properties mit klaren
   Unsupported-/Error-Zuständen implementieren.
4. **Texture-/Shader-Abdeckung ausbauen.** Die tatsächlich vom xemu-Core
   benötigten NV2A Texture-/PSH-/VSH-Register und Formatfälle anbinden; keine
   stillen Fallbacks auf „success“.
5. **Threadmodell festlegen und testen.** Ein D3D11-Owner, CoreWindow-
   Präsentationsqueue, Resize, Download-Events, Save/Shutdown und Device-Lost
   unter WARP und Hardware testen.
6. **UWP-App an xemu anschließen.** EntryPoint, Emulator-Initialisierung,
   Runloop, Storage, Input, Suspend/Resume, Networking und SDL-freie
   Abhängigkeiten integrieren.
7. **Meson-/MSBuild-Link-Closure bauen.** `xemu_uwp_host_dep` mit dem echten
   vollständigen xemu-UWP-Executable verbinden; Clean checkout in Parallels.
8. **Projekt/Manifest erstellen.** `xemu-uwp.vcxproj`, stabiles x64-
   `Package.appxmanifest`, Windows.Xbox, Capabilities, VCLibs und Assets.
9. **Parallels-Toolset ergänzen.** Erst nach Benutzerfreigabe das VS-UWP-
   Component installieren; danach authentische Configure-/Build-/Testmatrix.
10. **Unsigned package validieren, signieren, lokal installieren/starten.**
    MakeAppx/SignTool und AppContainer-Lauf müssen erfolgreich sein.
11. **Erst jetzt einen sparsamen manuellen GitHub-Lauf starten.** Release
    weiterhin false; Artifact genau prüfen.
12. **Benutzer-Hardware-Gate.** Xbox Device Portal installieren, starten,
    Telemetrie/Logs sichern und tatsächliches Spielverhalten prüfen.
13. **Erst nach allen Nachweisen committen/pushen und optional prerelease
    erzeugen.** Commit-Aufteilung nach AGENTS.md, keine unrelated Änderungen.

## 10. Definition of Done

Das Ziel darf erst als fertig bezeichnet werden, wenn **alle** Punkte durch
aktuelle Artefakte/Logs belegt sind:

- [ ] `validate-xbox-uwp.ps1` meldet `Ready=true` für das echte Projekt und
      nicht für den Host-Probe.
- [ ] x64-UWP-Executable enthält den vollständigen xemu-Core und linkt ohne
      SDL/Desktop-only-Abhängigkeit.
- [ ] D3D11 ist im PGRAPH tatsächlich auswählbar, registriert und initialisiert.
- [ ] Clear, Draw, Texture, PSH, VSH, Surface, Reports/Blits und notwendige
      PGRAPH-Zustände haben verifizierte produktive Pfade oder explizit
      nachgewiesene, für die Zielsoftware ausreichende Unsupported-Gates.
- [ ] Ein echter D3D11-Framebuffer wird als gültige borrowed Texture an den
      CoreWindow-Presenter übergeben, präsentiert und korrekt released.
- [ ] VRAM-Upload/Download, Dirty-Tracking, Flush, Resize, Scale,
      Device-Lost, Save/Shutdown und Thread-Lifetime bestehen unter Hardware
      D3D11 und WARP.
- [ ] UWP EntryPoint, Storage, Input, Suspend/Resume und benötigtes Networking
      sind im xemu-Prozess integriert und unter AppContainer getestet.
- [ ] x64-Manifest enthält Windows.Xbox, stabilen Publisher, VCLibs und nur
      erlaubte benötigte Capabilities.
- [ ] MakeAppx validiert das vollständige Paket; SignTool prüft Signatur und
      Publisher; PDB/SHA256/Metadata/SBOM sind vorhanden.
- [ ] Parallels installiert und startet das **vollständige xemu-Paket**, nicht
      nur den Probe; ein echter D3D11-Frame und die 13 Testartefakte sind belegt.
- [ ] Der Benutzer hat den Xbox-Series-X|S-Dev-Mode-Install/Start und den
      Hardwaretest durchgeführt und Logs/Ergebnis bestätigt.
- [ ] Danach erst werden Commit/Push und ein optionales klar markiertes
      Prerelease vorgenommen.

Bis diese Checkliste vollständig und aktuell belegt ist, lautet der korrekte
Status: **UWP/D3D11 Xbox-Build nicht fertig und nicht verifiziert**.

