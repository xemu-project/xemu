# OpenXbox — Networking Subsystem

This document covers the nvnet NIC emulation, System Link / LAN tunneling design, and the legal framework around online multiplayer.

---

## 1. Hardware Background

The Original Xbox NIC is an nVidia nForce Ethernet controller (the same family as the "nvnet" Linux driver). It sits on the MCPX south-bridge and is emulated in `hw/xbox/mcpx/nvnet/nvnet.c`.

### 1.1 OpenXbox nvnet improvements over upstream xemu

| Fix | Description |
|---|---|
| PROMISC mode | `NvRegPacketFilterFlags` PROMISC bit now correctly accepted and forwarded to the host backend. Required for System Link detection on some titles. |
| TX multi-packet | The transmit descriptor loop now processes all ready descriptors in one pass instead of stopping after the first. Reduces latency on burst-heavy titles. |
| RX/TX overflow | Ring-buffer full conditions are now handled gracefully (packet dropped + stats counter incremented) instead of asserting or silently corrupting state. |
| BIT1 control register | The undocumented BIT1 field in the control register is now preserved correctly across soft resets. |

---

## 2. System Link / LAN Tunneling

### 2.1 What System Link Is

System Link is the Original Xbox's LAN multiplayer mode. Games broadcast Xbox-specific UDP/IP packets (often on port 3074 or game-specific ports) as if all consoles were on the same Ethernet segment. There is no server-side authentication — discovery is purely broadcast-based.

### 2.2 Tunneling Approach

OpenXbox targets a **pure network-layer tunnel**: the emulator presents a virtual Ethernet adapter to the guest, and all frames sent by the guest are encapsulated and forwarded to a relay or peer over UDP/IP on the host network.

```
Guest (Xbox game)
  │  Ethernet frames (Xbox LAN broadcast)
  ▼
nvnet (emulated NIC)
  │  raw frame passed to host backend
  ▼
Tunnel backend (OpenXbox extension)
  │  UDP encapsulation
  ▼
Internet / LAN
  │
  ▼
Peer emulator instance (or physical Xbox via bridge)
  │  UDP decapsulation
  ▼
nvnet on peer
  │
  ▼
Guest (peer Xbox game)
```

### 2.3 Planned Tunnel Backends

| Backend | Status | Notes |
|---|---|---|
| **SLiRP (loopback)** | Upstream | Local-only; no peer discovery |
| **TAP device** | Upstream | Full bridge; requires root / admin |
| **XLink Kai compatibility** | Planned | Uses XLink Kai's existing relay network |
| **Insignia relay** | Planned | Insignia project's system-link relay server |
| **Built-in UDP tunnel** | Planned | Zero-config peer-to-peer, no external tool required |

### 2.4 Host Backend Integration

Tunnel backends are registered in `net/` using QEMU's existing `NetClientState` API. No guest-visible changes are required.

---

## 3. Legal Note

> **OpenXbox does not re-implement, bypass, or connect to native Xbox Live** (Microsoft's proprietary online service).

System Link / LAN tunneling operates entirely at the Ethernet-frame level. The game's own networking stack generates and consumes all packets. OpenXbox does not:

- Authenticate with Microsoft's servers.
- Decrypt, forge, or replay Xbox Live security tokens.
- Enable access to any licensed or paywalled online service.

This approach mirrors what physical Xbox consoles do when connected to the same LAN, tunneled over the internet via a VPN or dedicated relay. It is the same legal basis used by tools such as XLink Kai and Insignia system-link relay for the past 20+ years.

---

## 4. Packet Capture / Debugging

When debugging network issues, enable PCAP logging:

```bash
-object filter-dump,id=dump0,netdev=net0,file=/tmp/xbox-traffic.pcap \
-netdev tap,id=net0,ifname=tap0 \
-device nvnet,netdev=net0
```

Or use the TAP backend with `tcpdump`:

```bash
sudo tcpdump -i tap0 -w /tmp/xbox-traffic.pcap
```

---

## 5. Open Issues

- [ ] Broadcast packets are sometimes dropped by the host when SLiRP is the backend.
- [ ] PROMISC mode is not yet plumbed to the TAP backend on macOS.
- [ ] UDP tunnel backend is not yet implemented.
- [ ] mDNS-based peer discovery for zero-config LAN sessions is not yet designed.
