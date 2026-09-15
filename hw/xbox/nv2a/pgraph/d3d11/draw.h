//
// xemu NV2A D3D11 view-based triangle draw primitive
//
// Copyright (C) 2026 xemu Project
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#ifndef XEMU_NV2A_PGRAPH_D3D11_DRAW_H
#define XEMU_NV2A_PGRAPH_D3D11_DRAW_H

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>

#include <stddef.h>
#include <stdint.h>

#include <wrl/client.h>

namespace xemu {

enum class D3D11DrawStatus : uint8_t {
    Drawn,
    Invalid,
    Unsupported,
    ShaderError,
    WrongThread,
    DeviceError,
};

struct D3D11DrawResult {
    D3D11DrawStatus status = D3D11DrawStatus::Invalid;
    HRESULT hresult = E_INVALIDARG;
    HRESULT device_removed_reason = S_OK;

    bool succeeded() const
    {
        return status == D3D11DrawStatus::Drawn;
    }
};

/* CPU-owned data copied into short-lived D3D11_USAGE_IMMUTABLE buffers for
 * one Draw() call.  The caller retains ownership of data for the duration of
 * the call.  slot must match the input layout's InputSlot. */
struct D3D11DrawVertexBuffer {
    const void *data = nullptr;
    size_t byte_size = 0;
    UINT stride = 0;
    UINT slot = 0;
};

struct D3D11DrawIndexBuffer {
    const uint32_t *data = nullptr;
    size_t index_count = 0;
};

/**
 * Complete description of the deliberately small, view-based draw subset.
 * The only accepted topology is triangle-list.  The VS and PS fields must
 * contain offline DXBC produced by draw_shaders.ps1; runtime shader
 * compilation is intentionally not part of this primitive.
 *
 * All pointers are borrowed for Draw() and are never retained.  A null state
 * pointer means the D3D11 default state (the corresponding Set* call resets
 * the slot).  A null index buffer selects non-indexed Draw().  `vertex_count`
 * is required for Draw(), while `index_count` is required for DrawIndexed().
 */
struct D3D11DrawParams {
    ID3D11RenderTargetView *render_target = nullptr;
    ID3D11DepthStencilView *depth_stencil = nullptr;

    const void *vertex_shader_bytecode = nullptr;
    size_t vertex_shader_bytecode_size = 0;
    const void *pixel_shader_bytecode = nullptr;
    size_t pixel_shader_bytecode_size = 0;

    const D3D11_INPUT_ELEMENT_DESC *input_elements = nullptr;
    UINT input_element_count = 0;
    const D3D11DrawVertexBuffer *vertex_buffers = nullptr;
    UINT vertex_buffer_count = 0;
    D3D11DrawIndexBuffer index_buffer = {};

    UINT vertex_count = 0;
    UINT index_count = 0;
    UINT start_vertex = 0;
    UINT start_index = 0;
    INT base_vertex = 0;

    bool has_viewport = false;
    D3D11_VIEWPORT viewport = {};
    bool has_scissor = false;
    D3D11_RECT scissor = {};

    ID3D11RasterizerState *rasterizer_state = nullptr;
    ID3D11DepthStencilState *depth_stencil_state = nullptr;
    UINT stencil_ref = 0;
    ID3D11BlendState *blend_state = nullptr;
    FLOAT blend_factor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    UINT sample_mask = D3D11_DEFAULT_SAMPLE_MASK;
};

/**
 * Executes one triangle-list draw against explicit views.  This is an
 * internal primitive, not a PGRAPH renderer and not a surface-cache owner:
 * it does not mark surfaces dirty, flush VRAM, or mutate PGRAPH state.
 *
 * The immediate context is externally serialized by the caller.  Calls must
 * be made from the thread that constructed this object; this check catches a
 * common violation but does not replace external serialization.  Pipeline
 * state is changed by the draw and is not restored; callers that share the
 * context own state setup/serialization.
 */
class D3D11DrawPrimitive final {
public:
    D3D11DrawPrimitive(ID3D11Device *device, ID3D11DeviceContext *context);
    ~D3D11DrawPrimitive() = default;

    D3D11DrawPrimitive(const D3D11DrawPrimitive &) = delete;
    D3D11DrawPrimitive &operator=(const D3D11DrawPrimitive &) = delete;

    D3D11DrawResult Draw(const D3D11DrawParams &params) const;

private:
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    DWORD m_owner_thread = 0;
};

} // namespace xemu

#endif // _WIN32

#endif // XEMU_NV2A_PGRAPH_D3D11_DRAW_H
