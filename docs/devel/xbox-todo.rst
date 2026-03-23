Xbox Emulation — Future TODOs
==============================

This document tracks major improvement opportunities identified during
a codebase analysis of the Xbox-specific subsystems.  Items are grouped
by domain and sorted from highest to lowest estimated impact.

PFIFO / Command Processing (``hw/xbox/nv2a/pfifo.c``)
------------------------------------------------------

1. **Lock hierarchy refactor** *(lines 187, 222)* —
   The puller releases ``pfifo.lock``, acquires ``pgraph.lock``, then
   re-acquires ``pfifo.lock`` on return.  The existing ``TODO: this is
   fucked`` comments acknowledge the fragile ordering.  A proper lock
   hierarchy (or lock-free command ring) would eliminate the potential
   for priority inversion and reduce context-switch overhead.

2. **DMA busy-state tracking** *(line 269)* —
   ``NV_PFIFO_CACHE1_DMA_PUSH_STATE_BUSY`` is never set.  Real hardware
   asserts this while the pusher is actively consuming the push buffer;
   some titles may poll it.

PGRAPH / GPU Pipeline (``hw/xbox/nv2a/pgraph/``)
-------------------------------------------------

3. **Hardware context switching** *(pgraph.c line 198)* —
   ``TODO: hardware context switching`` — the current implementation
   only handles a single hardware channel; multi-channel support is
   stubbed out.

4. **Specular-power calculation** *(pgraph.c line 1875)* —
   ``FIXME: This handling is not correct`` — the specular lighting
   exponent path is known to produce slightly wrong results for some
   titles.

5. **CLEAR_SURFACE semantics** *(pgraph.c line 3083)* —
   ``FIXME: CLEAR_SURFACE seems to work like memset`` — the clear
   operation may bypass blend/stencil state in ways the emulator does
   not yet model.

6. **Query timestamp updates** *(pgraph.c lines 3144-3145)* —
   ``FIXME: Update timestamp?!`` / ``FIXME: Check`` — report-result
   writes may need a timestamp for correct GPU-query fence behaviour.

7. **Inline-element HW exception** *(pgraph.c line 2716)* —
   Real hardware throws an exception when the start index exceeds
   ``0xFFFF``; the emulator currently asserts instead of raising a
   proper GPU exception.

APU Voice Processor (``hw/xbox/mcpx/apu/vp/vp.c``)
----------------------------------------------------

8. **Decay / release envelope accuracy** *(lines 778-827)* —
   Multiple ``FIXME`` comments note that the exponential decay and
   release curves do not precisely match hardware.  A potential
   improvement is to switch to ``y(t) = 2^{-10t}`` as suggested in the
   inline comments, which would also enable a simple bit-shift
   attenuation.

9. **ADPCM decode buffer placement** *(line 898)* —
   ``TODO (Phase 9 perf): Move adpcm_decoded…`` — the decoded buffer is
   allocated in a sub-optimal location; moving it would improve cache
   locality during voice mixing.

10. **Cross-page sample reads** *(line 1059)* —
    ``FIXME: Handle reading across pages?!`` — voice sample fetches
    that straddle a 4 KiB page boundary are not handled and may produce
    audio glitches.

11. **Division-by-zero guard** *(line 721)* —
    When ``attack_rate == 0`` the amplitude envelope returns a hard
    ``255.0f``; hardware reportedly produces crackling.  The emulator
    should model the actual overflow behaviour.

NVNet Ethernet Controller (``hw/xbox/mcpx/nvnet/nvnet.c``)
-----------------------------------------------------------

12. **MII status mask** *(line 256)* —
    ``FIXME: MII status mask?`` — the PHY status register may need
    additional bits to satisfy link-detection polling in some titles.

13. **Broadcast filtering completeness** *(lines 588, 594)* —
    ``FIXME: bcast filtering`` / ``FIXME: Confirm PFF_MYADDR…`` —
    the receive-filter path does not yet fully implement all packet
    filter flag combinations.

DSP (``hw/xbox/mcpx/apu/dsp/dsp.c``)
--------------------------------------

14. **DMA timing model** *(line 174)* —
    ``FIXME: DMA timing be done cleaner`` — DMA transfers complete
    instantly; a cycle-accurate model would improve audio fidelity for
    latency-sensitive effects.

NV2A Core (``hw/xbox/nv2a/nv2a.c``)
-------------------------------------

15. **Early-startup race condition** *(line 149)* —
    ``FIXME: This case is sometimes hit during early Xbox startup.
    Presumably a race-condition…`` — an occasional spurious register
    access during boot could be guarded with a startup-ready flag.

16. **VRAM unref correctness** *(line 259)* —
    ``memory_region_unref(&vga->vram); // FIXME: Is this right?`` —
    the ownership semantics of the VGA VRAM region during NV2A teardown
    need verification to prevent a use-after-free.
