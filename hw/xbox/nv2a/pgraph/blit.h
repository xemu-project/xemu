// Header for blit.c

#pragma once

#include "hw/xbox/nv2a/nv2a_int.h"   // NV2AState, hwaddr, SurfaceBinding

typedef struct PGRAPHSurfaceOps {
    void (*surface_update)(NV2AState *d, bool, bool, bool);
    SurfaceBinding *(*surface_get)(NV2AState *d, hwaddr addr);
    void (*surface_download_if_dirty)(NV2AState *d, SurfaceBinding *surf);
} PGRAPHSurfaceOps;

void pgraph_common_image_blit(NV2AState *d, const PGRAPHSurfaceOps *ops);
