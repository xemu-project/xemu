//
// xemu NV2A D3D11 surface resources
//
// Copyright (C) 2026 xemu Project
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#ifndef XEMU_NV2A_PGRAPH_D3D11_SURFACE_H
#define XEMU_NV2A_PGRAPH_D3D11_SURFACE_H

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <wrl/client.h>

namespace xemu {

class D3D11PgraphContext;

/* This first D3D11 surface slice intentionally only covers the format and
 * layout combinations which can be represented without a shader conversion.
 * Bgr8A8 is the little-endian memory representation of NV097 A8R8G8B8. */
enum class D3D11SurfaceFormat : uint8_t {
    Bgr8A8,
    Z16,
    Z24S8,
};

enum class D3D11SurfaceType : uint8_t {
    LinearPitch,
};

enum class D3D11SurfaceAntialias : uint8_t {
    Center1,
};

/* Attachment ownership is metadata for cache-wide download reporting.  A
 * resource can be bound as both attachments, so the values are bit flags. */
enum class D3D11SurfaceAttachment : uint8_t {
    Unknown = 0,
    Color = 1,
    DepthStencil = 2,
};

inline D3D11SurfaceAttachment operator|(D3D11SurfaceAttachment left,
                                        D3D11SurfaceAttachment right)
{
    return static_cast<D3D11SurfaceAttachment>(static_cast<uint8_t>(left) |
                                               static_cast<uint8_t>(right));
}

inline D3D11SurfaceAttachment &operator|=(D3D11SurfaceAttachment &left,
                                          D3D11SurfaceAttachment right)
{
    left = left | right;
    return left;
}

inline bool HasSurfaceAttachment(D3D11SurfaceAttachment value,
                                 D3D11SurfaceAttachment attachment)
{
    return (static_cast<uint8_t>(value) & static_cast<uint8_t>(attachment)) !=
           0;
}

struct D3D11SurfaceDescriptor {
    uint64_t offset = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t pitch = 0;
    D3D11SurfaceFormat format = D3D11SurfaceFormat::Bgr8A8;
    D3D11SurfaceType type = D3D11SurfaceType::LinearPitch;
    uint32_t scale = 1;
    D3D11SurfaceAntialias antialias = D3D11SurfaceAntialias::Center1;

    bool operator==(const D3D11SurfaceDescriptor &other) const;
    bool operator!=(const D3D11SurfaceDescriptor &other) const
    {
        return !(*this == other);
    }
};

enum class D3D11SurfaceStatus : uint8_t {
    Ok,
    InvalidDescriptor,
    InvalidDevice,
    OutOfRange,
    Unsupported,
    DeviceLost,
    Timeout,
    Conflict,
    NotFound,
};

struct D3D11SurfaceResult {
    D3D11SurfaceStatus status = D3D11SurfaceStatus::InvalidDescriptor;
    HRESULT hresult = E_INVALIDARG;

    bool succeeded() const
    {
        return status == D3D11SurfaceStatus::Ok;
    }
};

struct D3D11SurfaceDownloadEvent {
    D3D11SurfaceDescriptor descriptor;
    uint64_t generation = 0;
    uint64_t size = 0;
    D3D11SurfaceAttachment attachment = D3D11SurfaceAttachment::Unknown;
};

/* A resource owns every D3D11 object used by one guest surface.  All object
 * accessors return borrowed pointers; the resource (or a cache Snapshot) must
 * outlive the caller's use of them.  The creating thread owns the immediate
 * context for this resource/cache; all D3D11 methods and borrowed native
 * accessors must be used on that thread (operations from another thread are
 * rejected).  GPU waits have a finite default timeout and report Timeout. */
class D3D11SurfaceResource final {
public:
    D3D11SurfaceResource(ID3D11Device *device, ID3D11DeviceContext *context,
                         const D3D11SurfaceDescriptor &descriptor,
                         uint64_t generation = 0,
                         uint32_t gpu_timeout_ms = 5000);
    ~D3D11SurfaceResource() = default;

    D3D11SurfaceResource(const D3D11SurfaceResource &) = delete;
    D3D11SurfaceResource &operator=(const D3D11SurfaceResource &) = delete;

    const D3D11SurfaceDescriptor &descriptor() const
    {
        return m_descriptor;
    }
    uint64_t generation() const
    {
        return m_generation;
    }
    bool valid() const
    {
        return m_ready && m_status != D3D11SurfaceStatus::DeviceLost &&
               m_status != D3D11SurfaceStatus::Timeout;
    }
    D3D11SurfaceStatus status() const
    {
        return m_status;
    }
    HRESULT last_hresult() const
    {
        return m_last_hresult;
    }
    HRESULT last_query_hresult() const
    {
        return m_last_query_hresult;
    }
    D3D11SurfaceStatus last_operation_status() const
    {
        return m_last_operation_status;
    }
    HRESULT last_operation_hresult() const
    {
        return m_last_operation_hresult;
    }

    uint32_t bytes_per_pixel() const
    {
        return m_bytes_per_pixel;
    }
    uint32_t row_bytes() const
    {
        return m_row_bytes;
    }
    uint64_t byte_end() const
    {
        return m_byte_end;
    }

    ID3D11Texture2D *texture() const
    {
        return m_texture.Get();
    }
    ID3D11RenderTargetView *render_target() const
    {
        return m_render_target.Get();
    }
    ID3D11DepthStencilView *depth_stencil() const
    {
        return m_depth_stencil.Get();
    }

    /* Upload/download copy only row_bytes, deliberately leaving guest pitch
     * padding untouched.  Both operations wait for GPU completion and report
     * device removal rather than returning a false success.  D3D11's void
     * CopyResource/Flush APIs are followed by an event query and
     * GetDeviceRemovedReason; asynchronous debug-layer failures can still be
     * reported only when the driver surfaces them. */
    bool Upload(const uint8_t *vram, size_t vram_size);
    bool Download(uint8_t *vram, size_t vram_size);

    /* Marking a draw requests a later GPU-to-VRAM download.  A draw while an
     * upload is pending, or an upload while a draw/download is pending, is a
     * Conflict and is rejected; this prevents one authority silently
     * overwriting the other.  The separate flags are exposed so a future
     * PGRAPH adapter can drive transfers without TCG callbacks. */
    bool MarkDrawDirty(bool dirty = true);
    bool MarkUploadDirty(bool dirty = true);
    bool MarkDownloadDirty(bool dirty = true);
    bool draw_dirty() const
    {
        return m_draw_dirty;
    }
    bool upload_dirty() const
    {
        return m_upload_dirty;
    }
    bool download_dirty() const
    {
        return m_download_dirty;
    }
    struct AuthoritySnapshot {
        bool draw_dirty;
        bool upload_dirty;
        bool download_dirty;
        D3D11SurfaceStatus status;
        HRESULT hresult;
        HRESULT query_hresult;
        D3D11SurfaceStatus operation_status;
        HRESULT operation_hresult;
    };

    AuthoritySnapshot SnapshotAuthority() const;
    void RestoreAuthority(const AuthoritySnapshot &snapshot);
    void RestoreTerminalFailure(D3D11SurfaceStatus status, HRESULT hresult,
                                HRESULT query_hresult,
                                D3D11SurfaceStatus operation_status,
                                HRESULT operation_hresult);
    void InjectDownloadFailureForTesting()
    {
        InjectDownloadFailureForTesting(D3D11SurfaceStatus::Unsupported,
                                        E_FAIL);
    }
    void InjectDownloadFailureForTesting(D3D11SurfaceStatus status,
                                         HRESULT hresult)
    {
        m_fail_download_for_testing = true;
        m_injected_download_status = status;
        m_injected_download_hresult = hresult;
    }

    /* Flush a pending draw/download request, if any. */
    bool Flush(uint8_t *vram, size_t vram_size);

private:
    bool CompleteGpuWork();
    bool CheckVramRange(size_t vram_size) const;
    bool CheckOwnerThread();
    bool ValidateInternalResources();
    void SetError(D3D11SurfaceStatus status, HRESULT hresult);
    void SetOperationError(D3D11SurfaceStatus status, HRESULT hresult);

    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_texture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_render_target;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depth_stencil;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_read_staging;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_write_staging;
    Microsoft::WRL::ComPtr<ID3D11Query> m_event_query;

    D3D11SurfaceDescriptor m_descriptor;
    uint64_t m_generation = 0;
    uint64_t m_byte_end = 0;
    uint32_t m_bytes_per_pixel = 0;
    uint32_t m_row_bytes = 0;
    D3D11SurfaceStatus m_status = D3D11SurfaceStatus::InvalidDescriptor;
    HRESULT m_last_hresult = E_INVALIDARG;
    HRESULT m_last_query_hresult = S_OK;
    D3D11SurfaceStatus m_last_operation_status = D3D11SurfaceStatus::Ok;
    HRESULT m_last_operation_hresult = S_OK;
    bool m_draw_dirty = false;
    bool m_upload_dirty = false;
    bool m_download_dirty = false;
    bool m_ready = false;
    uint32_t m_gpu_timeout_ms = 5000;
    uint32_t m_owner_thread = 0;
    bool m_fail_download_for_testing = false;
    D3D11SurfaceStatus m_injected_download_status =
        D3D11SurfaceStatus::Unsupported;
    HRESULT m_injected_download_hresult = E_FAIL;
};

/* A Snapshot retains the resource through a COM-owning shared object and
 * carries the generation observed at acquisition time.  A replacement in a
 * cache therefore cannot invalidate an in-flight renderer reference. */
class D3D11SurfaceSnapshot final {
public:
    D3D11SurfaceSnapshot() = default;
    explicit D3D11SurfaceSnapshot(
        std::shared_ptr<D3D11SurfaceResource> resource)
        : m_resource(std::move(resource)),
          m_generation(m_resource ? m_resource->generation() : 0)
    {
    }

    D3D11SurfaceResource *get()
    {
        return m_resource.get();
    }
    const D3D11SurfaceResource *get() const
    {
        return m_resource.get();
    }
    D3D11SurfaceResource *operator->()
    {
        return m_resource.get();
    }
    const D3D11SurfaceResource *operator->() const
    {
        return m_resource.get();
    }
    explicit operator bool() const
    {
        return m_resource != nullptr;
    }
    uint64_t generation() const
    {
        return m_generation;
    }
    std::shared_ptr<D3D11SurfaceResource> shared_resource()
    {
        return m_resource;
    }
    std::shared_ptr<const D3D11SurfaceResource> shared_resource() const
    {
        return m_resource;
    }

private:
    std::shared_ptr<D3D11SurfaceResource> m_resource;
    uint64_t m_generation = 0;
};

class D3D11SurfaceCache final {
public:
    D3D11SurfaceCache(ID3D11Device *device, ID3D11DeviceContext *context,
                      uint32_t gpu_timeout_ms = 5000);
    ~D3D11SurfaceCache() = default;

    D3D11SurfaceCache(const D3D11SurfaceCache &) = delete;
    D3D11SurfaceCache &operator=(const D3D11SurfaceCache &) = delete;

    bool valid() const
    {
        return m_device != nullptr && m_context != nullptr && m_valid_device &&
               !m_terminal;
    }
    D3D11SurfaceStatus status() const
    {
        return m_status;
    }
    HRESULT last_hresult() const
    {
        return m_last_hresult;
    }
    D3D11SurfaceStatus last_operation_status() const
    {
        return m_last_operation_status;
    }
    HRESULT last_operation_hresult() const
    {
        return m_last_operation_hresult;
    }

    /* Return the exact descriptor entry or replace all overlapping entries.
     * Before an overlapping entry is removed, dirty GPU data is downloaded to
     * the supplied guest VRAM range and an exact-once event is queued for the
     * owning PGRAPH context to harvest. */
    D3D11SurfaceSnapshot Acquire(
        const D3D11SurfaceDescriptor &descriptor, uint8_t *vram,
        size_t vram_size,
        D3D11SurfaceAttachment attachment = D3D11SurfaceAttachment::Unknown);
    D3D11SurfaceSnapshot Find(const D3D11SurfaceDescriptor &descriptor) const;
    /* Read-only descriptor lookup used while draining events after a terminal
     * failure.  Unlike Find, this does not hide entries solely because the
     * cache is terminal; it performs no D3D11 or cache mutation. */
    D3D11SurfaceSnapshot
    FindForInspection(const D3D11SurfaceDescriptor &descriptor) const;

    bool Flush(uint8_t *vram, size_t vram_size);
    size_t size() const
    {
        return m_entries.size();
    }

private:
    friend class D3D11PgraphContext;

    std::vector<D3D11SurfaceDownloadEvent> TakeDownloadEvents();
    void MarkTerminalFailure(D3D11SurfaceStatus status, HRESULT hresult);
    void RecordOperationFailure(D3D11SurfaceStatus status, HRESULT hresult);

    bool ValidateDescriptor(const D3D11SurfaceDescriptor &descriptor,
                            uint32_t *bytes_per_pixel, uint32_t *row_bytes,
                            uint64_t *byte_end) const;
    bool CheckOwnerThread() const;
    void SetError(D3D11SurfaceStatus status, HRESULT hresult);
    void SetOperationError(D3D11SurfaceStatus status, HRESULT hresult);

    struct Entry {
        D3D11SurfaceDescriptor descriptor;
        std::shared_ptr<D3D11SurfaceResource> resource;
        D3D11SurfaceAttachment attachment = D3D11SurfaceAttachment::Unknown;
    };

    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    std::vector<Entry> m_entries;
    std::vector<D3D11SurfaceDownloadEvent> m_download_events;
    uint64_t m_next_generation = 1;
    D3D11SurfaceStatus m_status = D3D11SurfaceStatus::Ok;
    HRESULT m_last_hresult = S_OK;
    D3D11SurfaceStatus m_last_operation_status = D3D11SurfaceStatus::Ok;
    HRESULT m_last_operation_hresult = S_OK;
    bool m_valid_device = false;
    bool m_terminal = false;
    uint32_t m_gpu_timeout_ms = 5000;
    uint32_t m_owner_thread = 0;
};

} // namespace xemu

#endif // _WIN32

#endif // XEMU_NV2A_PGRAPH_D3D11_SURFACE_H
