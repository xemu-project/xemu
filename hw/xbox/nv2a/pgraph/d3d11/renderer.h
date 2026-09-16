/*
 * xemu NV2A D3D11 PGRAPH renderer bridge.
 *
 * The PGRAPH callback table is owned by renderer.c, which is the only D3D11
 * file that includes the QEMU NV2A headers.  This header intentionally keeps
 * the C++ implementation behind an opaque state pointer so that the Windows
 * C++ translation units continue to use the narrow D3D11 bridge ABI.
 */

#ifndef XEMU_NV2A_PGRAPH_D3D11_RENDERER_H
#define XEMU_NV2A_PGRAPH_D3D11_RENDERER_H

#include <stdbool.h>
#include <stdint.h>

#include "d3d11_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct D3D11RendererState D3D11RendererState;

/* Creates the owner-thread D3D11 device and the shared surface-cache
 * executors.  The caller owns the returned state and must destroy it on the
 * same thread.  On failure, reason points to static storage. */
bool d3d11_renderer_state_init(NV2AState *state, D3D11RendererState **out_state,
                               const char **reason);
void d3d11_renderer_state_finalize(D3D11RendererState *state);

void d3d11_renderer_clear_surface(D3D11RendererState *renderer,
                                  NV2AState *state, uint32_t parameter);
void d3d11_renderer_draw(D3D11RendererState *renderer, NV2AState *state);
void d3d11_renderer_flush(D3D11RendererState *renderer, NV2AState *state);
void d3d11_renderer_surface_update(D3D11RendererState *renderer,
                                   NV2AState *state, bool upload,
                                   bool color_write, bool zeta_write);
void d3d11_renderer_flip_stall(D3D11RendererState *renderer, NV2AState *state);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* XEMU_NV2A_PGRAPH_D3D11_RENDERER_H */
