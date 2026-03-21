# OpenMidway Roadmap

OpenMidway is a user-friendly original Xbox emulator focused on multiplayer and
Windows usability. This roadmap describes the project's priority tiers and
development phases.

---

## Priority Tiers

### Tier 1 — What makes OpenMidway unique
- Multiplayer UX
- Windows experience
- Stability

### Tier 2
- GPU correctness
- Performance improvements

### Tier 3
- Deep accuracy (timing perfection, edge-case emulation)

---

## Phase 7 — Windows + Multiplayer UX

**Theme:** "Make playing Halo 2 online on Windows stupid easy"

- [x] One-click System Link setup
- [x] Auto MAC address handling
- [ ] Adapter auto-detection (Npcap)
- [ ] Multiplayer diagnostics panel
- [ ] Renderer auto-selection
- [x] "Host Game" button — creates a UDP tunnel automatically, generates a room
      code, opens ports / uses relay fallback

---

## Phase 8 — Graphics Fixes (Gameplay-first)

**Theme:** Fix visible breakage, not theoretical inaccuracy

Focus games:
- Halo 2
- Project Gotham Racing 2
- Fable
- Ninja Gaiden
- MechAssault

Deliverables:
- [ ] "Graphics Issues Mode" — per-game workaround toggles
- [ ] Shader cache improvements
- [ ] Fix flickering and unreadable UI in priority titles

---

## Phase 9 — Stability + Memory

**Theme:** Long sessions and multiplayer desync prevention

- [ ] Long-session stability improvements
- [ ] Memory leak audit
- [ ] DMA timing consistency
- [ ] Save-state safety (or explicit disable in multiplayer)
- [ ] "Multiplayer Safe Mode" — deterministic settings, disables risky
      optimizations, ensures sync between clients

---

## Phase 10 — OpenMidway Multiplayer Layer

**Theme:** The signature differentiator

### 10.1 Relay Server
- [ ] Room codes
- [ ] NAT traversal
- [ ] Fallback relay
- [ ] Latency display

### 10.2 Multiplayer UI
- [ ] Host / Join screen
- [ ] Friend-style UX
- [ ] Connection status indicator
- [ ] Game/version matching

### 10.3 Diagnostics
- [ ] "Why can't I join?" auto-checklist (MAC conflict, wrong version, NAT
      issue, adapter issue)

### 10.4 Voice Chat Improvements
- [ ] Build on existing communicator support
- [ ] Low-latency audio pipeline
- [ ] Push-to-talk
- [ ] Device selection

---

## Signature Features

| Feature | Description |
|---|---|
| **One-Click Multiplayer** | No port forwarding — room codes, auto config |
| **Multiplayer Diagnostics** | Tells you exactly what's wrong |
| **Windows-first polish** | Auto-detection, no janky setup |
| **Preset Modes** | Fast / Balanced / Accurate / Multiplayer Safe |
