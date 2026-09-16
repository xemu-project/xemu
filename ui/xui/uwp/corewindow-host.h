/*
 * SDL-free Windows UWP CoreWindow host.
 *
 * Copyright (C) 2026 xemu Project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef XEMU_UWP_COREWINDOW_HOST_H
#define XEMU_UWP_COREWINDOW_HOST_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* These are deliberately opaque in the public ABI. The implementation is
 * built with the Windows UWP projection and owns all COM references. */
typedef struct ID3D11Device ID3D11Device;
typedef struct ID3D11DeviceContext ID3D11DeviceContext;
typedef struct ID3D11Texture2D ID3D11Texture2D;
typedef struct IUnknown IUnknown;

typedef struct XemuUwpCoreCallbacks {
    void *opaque;

    /* Called on the CoreWindow thread after the D3D11 device and swap chain
     * have been created. The callback must not retain the COM pointers. */
    bool (*initialize)(void *opaque, ID3D11Device *device,
                       ID3D11DeviceContext *context, IUnknown *core_window,
                       uint32_t width, uint32_t height);

    /* Render one frame on the CoreWindow thread. On success, *texture is a
     * borrowed ID3D11Texture2D from the same device passed to initialize().
     * The host copies it into the CoreWindow back buffer before Present(). */
    bool (*render)(void *opaque, ID3D11Texture2D **texture);

    /* Called after CoreWindow::SizeChanged has resized the swap chain. */
    bool (*resize)(void *opaque, uint32_t width, uint32_t height);

    /* Called exactly once before the host returns, on the CoreWindow thread. */
    void (*shutdown)(void *opaque);
} XemuUwpCoreCallbacks;

/* Runs the UWP CoreApplication loop. The caller supplies the application
 * callbacks; no SDL, HWND, or desktop message pump is used. */
int xemu_uwp_run(const XemuUwpCoreCallbacks *callbacks);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* XEMU_UWP_COREWINDOW_HOST_H */
