# xemu Xbox Series X|S Dev Mode – Projektplan & Architektur

Stand: 2026-09-16  
Status: In aktiver Umsetzung  

Dieses Dokument definiert den verbindlichen technischen Gesamtplan, die Architektur und die Phasen-Roadmap für die Portierung von **xemu auf die Xbox Series X|S im Developer Mode** als schlanke, native x64-UWP-Applikation (`.msix`).

---

## 1. Leitbild & Design-Entscheidungen

### 1.1 Fokus: Schneller Pfad zu `.xiso.iso`-Ausführung
* **Keine komplexe Desktop-UI**: Eine Portierung der Desktop-Menüs (ImGui, Menüleisten, xui-Settings) auf UWP/CoreWindow ist für das Ziel nicht erforderlich und wird bewusst übersprungen.
* **Schlanker Start**: Beim App-Start wird entweder ein einfacher UWP-`FileOpenPicker` geöffnet oder automatisch eine Standard-Datei geladen (`LocalFolder\game.iso` bzw. von einem angeschlossenen USB-Laufwerk `D:\game.iso`).
* **Reine x64-Architektur**: Die Xbox Series X|S nutzt AMD Zen 2 (x64). Weder ARM64 noch x86-32 werden für das Xbox-Paket unterstützt.
* **SDL- und Desktop-Freiheit**: Keine Win32-Desktop-APIs (`HWND`, `wgl`, Desktop-Mauszeiger, etc.), die in der UWP-AppContainer-Sandbox verboten sind oder abstürzen.

---

## 2. Architekturübersicht

```
+-------------------------------------------------------------------------+
|                       Xbox Series X|S (Dev Mode)                        |
|                                                                         |
|  +-------------------------------------------------------------------+  |
|  |                 UWP AppContainer (x64 MSIX)                       |  |
|  |                                                                   |  |
|  |  +-------------------------+      +----------------------------+  |  |
|  |  |   CoreWindow & Host     |      |       Storage & Input      |  |  |
|  |  | (corewindow-host.cc)    |      |                            |  |  |
|  |  | - IFrameworkView / Run  |      | - LocalFolder (BIOS/ROMs)  |  |  |
|  |  | - D3D11 SwapChain       |      | - FileOpenPicker / USB ISO |  |  |
|  |  | - Presenter-Handoff     |      | - Windows.Gaming.Input     |  |  |
|  |  +------------+------------+      +-------------+--------------+  |  |
|  |               |                                 |                 |  |
|  |               v                                 v                 |  |
|  |  +-------------------------------------------------------------+  |  |
|  |  |                  xemu Emulator Core                         |  |  |
|  |  |                                                             |  |  |
|  |  | - x86 CPU / TCG / Memory Controller                         |  |  |
|  |  | - MCPX Boot ROM & BIOS Initialisierung                     |  |  |
|  |  | - IDE / DVD Block Device (-dvd_path)                        |  |  |
|  |  | - Virtual Xbox Gamepad Hub                                  |  |  |
|  |  +------------------------------+------------------------------+  |  |
|  |                                 |                                 |  |
|  |                                 v                                 |  |
|  |  +-------------------------------------------------------------+  |  |
|  |  |              NV2A / PGRAPH Direct3D 11 Renderer             |  |  |
|  |  |                                                             |  |  |
|  |  | - Single Owner Immediate Context (D3D11 Device / Context)   |  |  |
|  |  | - Surface Cache & RenderTarget -> ID3D11Texture2D Lease     |  |  |
|  |  | - Swizzled Texture Upload -> D3D11 Shader Resource Views    |  |  |
|  |  | - Vertex Shader (VSH) & Pixel Shader / Combiner (PSH)       |  |  |
|  |  | - Clear, Draw & Pushbuffer Processing                      |  |  |
|  |  +-------------------------------------------------------------+  |  |
|  +-------------------------------------------------------------------+  |
+-------------------------------------------------------------------------+
```

---

## 3. Phasen-Roadmap

### Phase 1: UWP-Projektdatei & GitHub Actions Packaging (Aktuell)
* **Ziel**: Vollständige Projekt- und Manifest-Struktur bereitstellen, die alle Kriterien von `scripts/ci/validate-xbox-uwp.ps1` erfüllt.
* **Artefakte**:
  - `ui/xui/uwp/xemu-uwp.vcxproj` (UWP x64 Projekt mit `<XemuUwpTarget>true</XemuUwpTarget>`).
  - `ui/xui/uwp/Package.appxmanifest` (Identity `xemu.Xbox`, `Windows.Xbox` DeviceFamily, `Microsoft.VCLibs.140.00`).
  - `ui/xui/uwp/main.cpp` (MTAThread App-Einstiegspunkt).
  - `ui/xui/uwp/assets/` (Kachel-Icons).
* **Build-Entlastung**: Lokale Parallels-VM validiert nur strukturell; das Kompilieren und Packen des x64-MSIX wird an GitHub Actions (`.github/workflows/build-xbox-uwp.yml`) delegiert.

### Phase 2: UWP-App-Host & Dateipfad-Integration
* **Ziel**: Starten des xemu-Cores aus der UWP-App heraus mit den benötigten ROM-/ISO-Dateien.
* **Aufgaben**:
  1. **Sandbox-Storage auflösen**:
     - `LocalFolder` prüfen auf `mcpx_rom.bin`, `bios.bin` und `xbox_hdd.qcow2`.
     - Bei Fehlen: Hinweistext oder Bereitstellung über `Package::Current->InstalledLocation`.
  2. **ISO-Auswahl**:
     - Option A: UWP `FileOpenPicker` (Filter `.iso`, `.xiso`) asynchron beim Start aufrufen.
     - Option B: Automatische Erkennung einer `game.iso` im `LocalFolder` oder im Stammverzeichnis eines USB-Speichersticks (`D:\`).
  3. **Emulator-Worker-Thread**:
     - Entkopplung des QEMU/xemu-Main-Loops vom UWP `CoreWindow`-UI-Thread.
     - Starten von xemu mit den Argumenten `-bios`, `-dvd_path`, etc. ohne Desktop-SDL.

### Phase 3: PGRAPH D3D11-Renderer & Framebuffer-Handoff (Gate B)
* **Ziel**: Der Emulator rendert ein Bild, das über die D3D11-Swapchain auf dem Bildschirm dargestellt wird.
* **Aufgaben**:
  1. **Framebuffer-Lease**:
     - `hw/xbox/nv2a/pgraph/d3d11/renderer.cc`: In `get_framebuffer_surface` ein echtes `NV2AFramebufferSurface` mit `ID3D11Texture2D` zurückliefern (bisher Dummy `false`).
  2. **Context- & Device-Owner**:
     - Sicherstellen, dass der D3D11 Immediate Context und die Ressourcen thread-sicher vom Render-Thread verwaltet werden.
  3. **Presenter-Copy**:
     - Der CoreWindow-Host ruft `callbacks.render()` auf, übernimmt die Textur via `presenter_->CopyTexture(texture)` und führt `presenter_->Present(1, 0)` aus.

### Phase 4: NV2A Texture- & Shader-Pipeline (Gate C)
* **Ziel**: Spiele können Texturen und Shader nutzen, ohne abzustürzen oder nur schwarze Polygone zu rendern.
* **Aufgaben**:
  1. **Textur-Upload & Formate**:
     - Swizzled/Linear Texture Deserialisierung in D3D11 2D-Texturen.
     - Unterstützung für Standard-Formate: DXT1, DXT3, DXT5, A8R8G8B8, X8R8G8B8.
  2. **Vertex Shader (VSH)**:
     - CPU-Fallback / DXBC-Shader-Generierung für Standard-Transform-Modi und programmierbare VSH.
  3. **Pixel Shader / Combiner (PSH)**:
     - NV2A Register Combiner Abbildung auf D3D11 Pixel Shader.

### Phase 5: Controller & Audio
* **Ziel**: Spiele können mit dem Xbox Wireless Controller gesteuert werden und erzeugen Sound.
* **Aufgaben**:
  1. **Gamepad-Eingabe**:
     - `Windows.Gaming.Input.Gamepad` Events / Polling abgreifen.
     - Weiterleitung der Buttons, Trigger und Analogsticks an den xemu virtuellen USB-Gamepad-Hub (`hw/xbox/usb.c`).
  2. **Audio-Ausgabe**:
     - SDL-Audio durch schlanken WASAPI-Renderer oder XAudio2 für den UWP-AppContainer ersetzen.

### Phase 6: Paketierung, Signierung & Xbox-Deployment
* **Ziel**: Vollständige Installation und Verifikation auf Xbox Series X|S Dev Mode.
* **Aufgaben**:
  1. GitHub Actions Build triggern (`Release`, `x64`, `pfx_required` oder `dev_test_certificate`).
  2. Download des generierten `.zip` Bundles (enthält MSIX, VCLibs, Zertifikat).
  3. Installation über das Xbox Device Portal (`https://<xbox-ip>:11443`).
  4. Testboot eines ISOs und Dokumentation der Logs.

---

## 4. Richtlinien für die Umsetzung

1. **Single Responsibility**: Jede Änderung konzentriert sich auf ein spezifisches Gate (z.B. UWP-Projekt, Framebuffer-Export, Input).
2. **Minimal Change**: Keine unnötigen Header-Verschiebungen oder Umformatierungen bestehender QEMU/xemu-Dateien.
3. **Strenge Validierung**:
   - Vor jedem Push lokal: `scripts/ci/validate-xbox-uwp.ps1`.
   - C++ Unit-Tests für D3D11 / Bridge-Module grün halten.
   - White-Space-Prüfung mit `git diff --check`.
