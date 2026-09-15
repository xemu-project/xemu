#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <processthreadsapi.h>
#include <wrl.h>
#include <wrl/wrappers/corewrappers.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <roapi.h>
#include <collection.h>
#include <ppltasks.h>
#include <windows.gaming.input.h>
#include <windows.applicationmodel.core.h>
#include <windows.storage.h>
#include <windows.ui.core.h>
#include "d3d11-corewindow-present.h"
#include "d3d11-present.h"
#include "clear.h"
#include "draw.h"
#include "draw_shaders.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>
#include <string>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using namespace Microsoft::WRL;
using namespace Platform;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Gaming::Input;
using namespace Windows::Storage;
using namespace Windows::UI::Core;

namespace {
std::wstring g_logPath;

std::string JsonEscape(const std::string &value)
{
    std::string result;
    static constexpr char hex[] = "0123456789abcdef";
    for (unsigned char c : value) {
        switch (c) {
        case '\\':
            result += "\\\\";
            break;
        case '"':
            result += "\\\"";
            break;
        case '\b':
            result += "\\b";
            break;
        case '\f':
            result += "\\f";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            if (c < 0x20) {
                result += "\\u00";
                result += hex[c >> 4];
                result += hex[c & 0x0f];
            } else {
                result += static_cast<char>(c);
            }
        }
    }
    return result;
}

void Log(const char *name, bool ok, const char *detail = "")
{
    const std::string escapedName = JsonEscape(name ? name : "");
    const std::string escapedDetail = JsonEscape(detail ? detail : "");
    std::string line = "{\"test\":\"" + escapedName +
                       "\",\"ok\":" + (ok ? "true" : "false") +
                       ",\"detail\":\"" + escapedDetail + "\"}\r\n";
    OutputDebugStringA(line.c_str());
    FILE *file = nullptr;
    if (!g_logPath.empty() && _wfopen_s(&file, g_logPath.c_str(), L"ab") == 0 &&
        file) {
        fwrite(line.data(), 1, line.size(), file);
        fflush(file);
        fclose(file);
    }
}

void LogPixel(const char *name, bool ok, UINT x, UINT y, const char *expected,
              const char *actual)
{
    char line[384];
    sprintf_s(line,
              "{\"test\":\"%s\",\"ok\":%s,\"x\":%u,\"y\":%u,"
              "\"expected\":\"%s\",\"actual\":\"%s\"}\r\n",
              name, ok ? "true" : "false", x, y, expected, actual);
    OutputDebugStringA(line);
    FILE *file = nullptr;
    if (!g_logPath.empty() && _wfopen_s(&file, g_logPath.c_str(), L"ab") == 0 &&
        file) {
        fwrite(line, 1, strlen(line), file);
        fflush(file);
        fclose(file);
    }
}

const char *ClearStatusName(xemu::D3D11ClearStatus status)
{
    switch (status) {
    case xemu::D3D11ClearStatus::Cleared:
        return "cleared";
    case xemu::D3D11ClearStatus::Unsupported:
        return "unsupported";
    case xemu::D3D11ClearStatus::Invalid:
        return "invalid";
    case xemu::D3D11ClearStatus::DeviceError:
        return "device_error";
    }
    return "unknown";
}

const char *DrawStatusName(xemu::D3D11DrawStatus status)
{
    switch (status) {
    case xemu::D3D11DrawStatus::Drawn:
        return "drawn";
    case xemu::D3D11DrawStatus::Invalid:
        return "invalid";
    case xemu::D3D11DrawStatus::Unsupported:
        return "unsupported";
    case xemu::D3D11DrawStatus::ShaderError:
        return "shader_error";
    case xemu::D3D11DrawStatus::WrongThread:
        return "wrong_thread";
    case xemu::D3D11DrawStatus::DeviceError:
        return "device_error";
    }
    return "unknown";
}

const char *FeatureLevelName(D3D_FEATURE_LEVEL level)
{
    switch (level) {
    case D3D_FEATURE_LEVEL_11_1:
        return "11_1";
    case D3D_FEATURE_LEVEL_11_0:
        return "11_0";
    case D3D_FEATURE_LEVEL_10_1:
        return "10_1";
    case D3D_FEATURE_LEVEL_10_0:
        return "10_0";
    case D3D_FEATURE_LEVEL_9_3:
        return "9_3";
    case D3D_FEATURE_LEVEL_9_2:
        return "9_2";
    case D3D_FEATURE_LEVEL_9_1:
        return "9_1";
    default:
        return "unknown";
    }
}

void LogDrawResult(const xemu::D3D11DrawResult &result,
                   D3D_FEATURE_LEVEL feature_level, HRESULT device_hresult,
                   bool gpu_completed, bool observed_readback, bool inside_ok,
                   bool outside_ok)
{
    char line[640] = {};
    const HRESULT hr = FAILED(result.hresult) ? result.hresult : device_hresult;
    sprintf_s(line,
              "{\"test\":\"d3d11_draw_after_clear\",\"ok\":%s,"
              "\"status\":\"%s\",\"hresult\":\"0x%08lx\","
              "\"feature_level\":\"%s\",\"gpu_completed\":%s,"
              "\"observed_readback\":%s,\"inside\":%s,"
              "\"outside\":%s}\r\n",
              result.succeeded() && gpu_completed && observed_readback &&
                      inside_ok && outside_ok ?
                  "true" :
                  "false",
              DrawStatusName(result.status), static_cast<unsigned long>(hr),
              FeatureLevelName(feature_level), gpu_completed ? "true" : "false",
              observed_readback ? "true" : "false",
              inside_ok ? "true" : "false", outside_ok ? "true" : "false");
    OutputDebugStringA(line);
    FILE *file = nullptr;
    if (!g_logPath.empty() && _wfopen_s(&file, g_logPath.c_str(), L"ab") == 0 &&
        file) {
        fwrite(line, 1, strlen(line), file);
        fflush(file);
        fclose(file);
    }
}

void LogClearResult(const char *name, const xemu::D3D11ClearResult &result,
                    xemu::D3D11ClearStatus expected, bool observed)
{
    char line[384] = {};
    sprintf_s(line,
              "{\"test\":\"%s\",\"ok\":%s,\"status\":\"%s\","
              "\"expected_status\":\"%s\",\"observed\":%s,"
              "\"hresult\":\"0x%08lx\"}\r\n",
              name, observed ? "true" : "false", ClearStatusName(result.status),
              ClearStatusName(expected), observed ? "true" : "false",
              static_cast<unsigned long>(result.hresult));
    OutputDebugStringA(line);
    FILE *file = nullptr;
    if (!g_logPath.empty() && _wfopen_s(&file, g_logPath.c_str(), L"ab") == 0 &&
        file) {
        fwrite(line, 1, strlen(line), file);
        fflush(file);
        fclose(file);
    }
}

void ProbeCodeGeneration()
{
    PROCESS_MITIGATION_DYNAMIC_CODE_POLICY policy = {};
    if (!GetProcessMitigationPolicy(GetCurrentProcess(),
                                    ProcessDynamicCodePolicy, &policy,
                                    sizeof(policy))) {
        Log("dynamic_code_policy", false, "GetProcessMitigationPolicy failed");
        Log("rw_to_rx_execute", false,
            "skipped because dynamic-code policy is unknown");
        return;
    }
    if (policy.ProhibitDynamicCode) {
        Log("dynamic_code_policy", false,
            "prohibit dynamic code is enabled; execution skipped");
        Log("rw_to_rx_execute", false,
            "skipped by process dynamic-code policy");
        return;
    }
    Log("dynamic_code_policy", true, "dynamic code is permitted");
    const unsigned char code[] = { 0xB8, 0x2A, 0x00, 0x00, 0x00, 0xC3 };
    void *memory =
        VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    DWORD oldProtect = 0;
    bool ok = memory && memcpy(memory, code, sizeof(code)) &&
              VirtualProtect(memory, 4096, PAGE_EXECUTE_READ, &oldProtect);
    int value = ok ? reinterpret_cast<int (*)()>(memory)() : 0;
    Log("rw_to_rx_execute", ok && value == 42,
        ok ? "returned 42" : "allocation/protection/execute failed");
    if (memory)
        VirtualFree(memory, 0, MEM_RELEASE);
}

void ProbeAddressSpace()
{
    constexpr SIZE_T size = 4ull * 1024 * 1024 * 1024;
    void *memory = VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_NOACCESS);
    Log("large_address_reservation", memory != nullptr,
        memory ? "reserved 4 GiB" : "4 GiB reservation failed");
    if (memory)
        VirtualFree(memory, 0, MEM_RELEASE);
}

thread_local int tlsValue = 0;
DWORD WINAPI ProbeThread(void *)
{
    tlsValue = 7;
    return tlsValue == 7 ? 0 : 1;
}

void ProbeThreadsAndTls()
{
    HANDLE thread = CreateThread(nullptr, 0, ProbeThread, nullptr, 0, nullptr);
    DWORD result = thread ? WaitForSingleObject(thread, 5000) : WAIT_FAILED;
    DWORD exitCode = 1;
    if (thread) {
        GetExitCodeThread(thread, &exitCode);
        CloseHandle(thread);
    }
    Log("threads_tls", result == WAIT_OBJECT_0 && exitCode == 0,
        "CreateThread + thread_local round trip");
}

void ProbeStorage()
{
    bool ok = false;
    try {
        auto folder = ApplicationData::Current->LocalFolder;
        auto file =
            concurrency::create_task(
                folder->CreateFileAsync("xemu-uwp-host-probe.jsonl",
                                        CreationCollisionOption::OpenIfExists))
                .get();
        g_logPath = file->Path->Data();
        ok = !g_logPath.empty();
    } catch (Exception ^) {
        g_logPath.clear();
    }
    Log("app_local_storage", ok,
        ok ? "LocalFolder available" : "LocalFolder unavailable");
}

void ProbeGamepads()
{
    unsigned count = 0;
    bool ok = true;
    try {
        count = Gamepad::Gamepads->Size;
    } catch (Exception ^) {
        ok = false;
    }
    char detail[80];
    if (ok) {
        sprintf_s(detail, "%u gamepad(s) enumerated", count);
    } else {
        strcpy_s(detail, "Gamepad::Gamepads enumeration threw an exception");
    }
    Log("gamepad_enumeration", ok, detail);
}

bool CreateColorTarget(ID3D11Device *device, UINT width, UINT height,
                       DXGI_FORMAT format, ComPtr<ID3D11Texture2D> *texture,
                       ComPtr<ID3D11RenderTargetView> *view)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    return SUCCEEDED(device->CreateTexture2D(
               &desc, nullptr, texture->ReleaseAndGetAddressOf())) &&
           SUCCEEDED(device->CreateRenderTargetView(
               texture->Get(), nullptr, view->ReleaseAndGetAddressOf()));
}

bool CreateDepthTarget(ID3D11Device *device, UINT width, UINT height,
                       DXGI_FORMAT resource_format, DXGI_FORMAT view_format,
                       ComPtr<ID3D11Texture2D> *texture,
                       ComPtr<ID3D11DepthStencilView> *view)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = resource_format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(device->CreateTexture2D(&desc, nullptr,
                                       texture->ReleaseAndGetAddressOf()))) {
        return false;
    }
    D3D11_DEPTH_STENCIL_VIEW_DESC view_desc = {};
    view_desc.Format = view_format;
    view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    return SUCCEEDED(device->CreateDepthStencilView(
        texture->Get(), &view_desc, view->ReleaseAndGetAddressOf()));
}

bool ReadbackTexture(ID3D11Device *device, ID3D11DeviceContext *context,
                     ID3D11Texture2D *source, UINT bytes_per_pixel,
                     std::vector<uint8_t> *bytes, UINT *row_bytes)
{
    if (device == nullptr || context == nullptr || source == nullptr ||
        bytes == nullptr || row_bytes == nullptr) {
        return false;
    }
    D3D11_TEXTURE2D_DESC desc = {};
    source->GetDesc(&desc);
    if (desc.Width == 0 || desc.Height == 0 || bytes_per_pixel == 0 ||
        desc.Width > std::numeric_limits<UINT>::max() / bytes_per_pixel) {
        return false;
    }
    const UINT row_size = desc.Width * bytes_per_pixel;
    D3D11_TEXTURE2D_DESC staging_desc = desc;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.BindFlags = 0;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging_desc.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(device->CreateTexture2D(&staging_desc, nullptr, &staging))) {
        return false;
    }
    context->CopyResource(staging.Get(), source);
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)) ||
        mapped.pData == nullptr || mapped.RowPitch < row_size) {
        return false;
    }
    const size_t total = static_cast<size_t>(row_size) * desc.Height;
    bytes->resize(total);
    for (UINT y = 0; y < desc.Height; ++y) {
        memcpy(bytes->data() + static_cast<size_t>(y) * row_size,
               static_cast<const uint8_t *>(mapped.pData) +
                   static_cast<size_t>(y) * mapped.RowPitch,
               row_size);
    }
    context->Unmap(staging.Get(), 0);
    *row_bytes = row_size;
    return true;
}

bool CheckColorPixel(const std::vector<uint8_t> &bytes, UINT row_bytes, UINT x,
                     UINT y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    const uint8_t *pixel = bytes.data() + static_cast<size_t>(y) * row_bytes +
                           static_cast<size_t>(x) * 4u;
    return pixel[0] == r && pixel[1] == g && pixel[2] == b && pixel[3] == a;
}

bool CheckDepthStencilPixel(const std::vector<uint8_t> &bytes, UINT row_bytes,
                            UINT x, UINT y, uint32_t depth, uint8_t stencil,
                            bool has_stencil)
{
    const uint8_t *pixel = bytes.data() + static_cast<size_t>(y) * row_bytes +
                           static_cast<size_t>(x) * 4u;
    const bool depth_ok = pixel[0] == static_cast<uint8_t>(depth) &&
                          pixel[1] == static_cast<uint8_t>(depth >> 8) &&
                          pixel[2] == static_cast<uint8_t>(depth >> 16);
    return depth_ok && (!has_stencil || pixel[3] == stencil);
}

struct DrawProbeSurface {
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11RenderTargetView> view;
    xemu::D3D11DrawResult result = {};
    bool gpu_completed = false;
    bool observed_readback = false;
    bool inside_ok = false;
    bool outside_ok = false;
};

bool WaitForDrawGpuCompletion(ID3D11Device *device,
                              ID3D11DeviceContext *context)
{
    if (device == nullptr || context == nullptr) {
        return false;
    }
    D3D11_QUERY_DESC query_desc = {};
    query_desc.Query = D3D11_QUERY_EVENT;
    ComPtr<ID3D11Query> query;
    if (FAILED(device->CreateQuery(&query_desc, &query))) {
        return false;
    }
    context->End(query.Get());
    context->Flush();
    const ULONGLONG deadline = GetTickCount64() + 5000;
    for (;;) {
        const HRESULT hr = context->GetData(query.Get(), nullptr, 0,
                                            D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if (hr == S_OK) {
            return true;
        }
        if (FAILED(hr) || GetTickCount64() >= deadline) {
            return false;
        }
        Sleep(1);
    }
}

bool ProbeD3D11Draw(ID3D11Device *device, ID3D11DeviceContext *context,
                    D3D_FEATURE_LEVEL feature_level, HRESULT device_hresult,
                    UINT width, UINT height, DrawProbeSurface *surface)
{
    if (surface == nullptr) {
        return false;
    }
    surface->texture.Reset();
    surface->view.Reset();
    surface->result = {};
    surface->gpu_completed = false;
    surface->observed_readback = false;
    surface->inside_ok = false;
    surface->outside_ok = false;
    try {
        if (device == nullptr || context == nullptr || width == 0 ||
            height == 0) {
            LogDrawResult(surface->result, feature_level, device_hresult, false,
                          false, false, false);
            return false;
        }

        if (!CreateColorTarget(device, width, height,
                               DXGI_FORMAT_B8G8R8A8_UNORM, &surface->texture,
                               &surface->view)) {
            LogDrawResult(surface->result, feature_level, device_hresult, false,
                          false, false, false);
            return false;
        }
        const FLOAT clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        context->ClearRenderTargetView(surface->view.Get(), clear_color);

        D3D11_RASTERIZER_DESC rasterizer_desc = {};
        rasterizer_desc.FillMode = D3D11_FILL_SOLID;
        rasterizer_desc.CullMode = D3D11_CULL_BACK;
        rasterizer_desc.FrontCounterClockwise = FALSE;
        rasterizer_desc.DepthClipEnable = TRUE;
        ComPtr<ID3D11RasterizerState> rasterizer_state;
        if (FAILED(device->CreateRasterizerState(&rasterizer_desc,
                                                 &rasterizer_state))) {
            LogDrawResult(surface->result, feature_level, device_hresult, false,
                          false, false, false);
            return false;
        }

        struct Vertex {
            float position[3];
            float color[4];
        };
        /* With D3D's top-left viewport convention, this clockwise clip-space
         * order is front-facing for FrontCounterClockwise == FALSE. */
        const Vertex vertices[] = {
            { { -0.80f, -0.80f, 0.5f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
            { { 0.00f, 0.80f, 0.5f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
            { { 0.80f, -0.80f, 0.5f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
        };
        const uint32_t indices[] = { 0, 1, 2 };
        const D3D11_INPUT_ELEMENT_DESC input_elements[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
              D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
              D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        const xemu::D3D11DrawVertexBuffer vertex_buffer = { vertices,
                                                            sizeof(vertices),
                                                            sizeof(Vertex), 0 };
        xemu::D3D11DrawParams params = {};
        params.render_target = surface->view.Get();
        params.vertex_shader_bytecode = xemu::d3d11_draw_shaders::kVertexShader;
        params.vertex_shader_bytecode_size =
            xemu::d3d11_draw_shaders::kVertexShaderSize;
        params.pixel_shader_bytecode = xemu::d3d11_draw_shaders::kPixelShader;
        params.pixel_shader_bytecode_size =
            xemu::d3d11_draw_shaders::kPixelShaderSize;
        params.input_elements = input_elements;
        params.input_element_count = _countof(input_elements);
        params.vertex_buffers = &vertex_buffer;
        params.vertex_buffer_count = 1;
        params.index_buffer = { indices, _countof(indices) };
        params.index_count = _countof(indices);
        params.has_viewport = true;
        params.viewport = {
            0.0f, 0.0f, static_cast<FLOAT>(width), static_cast<FLOAT>(height),
            0.0f, 1.0f
        };
        params.rasterizer_state = rasterizer_state.Get();

        xemu::D3D11DrawPrimitive primitive(device, context);
        surface->result = primitive.Draw(params);
        if (!surface->result.succeeded()) {
            LogDrawResult(surface->result, feature_level, device_hresult, false,
                          false, false, false);
            return false;
        }
        surface->gpu_completed = WaitForDrawGpuCompletion(device, context);
        if (!surface->gpu_completed) {
            surface->result.status = xemu::D3D11DrawStatus::DeviceError;
            surface->result.hresult = DXGI_ERROR_WAS_STILL_DRAWING;
            surface->result.device_removed_reason =
                device->GetDeviceRemovedReason();
            LogDrawResult(surface->result, feature_level, device_hresult, false,
                          false, false, false);
            return false;
        }

        std::vector<uint8_t> bytes;
        UINT row_bytes = 0;
        surface->observed_readback = ReadbackTexture(
            device, context, surface->texture.Get(), 4, &bytes, &row_bytes);
        if (surface->observed_readback && width >= 4 && height >= 4) {
            const UINT inside_x = width / 2;
            const UINT inside_y = height / 2;
            surface->inside_ok = CheckColorPixel(bytes, row_bytes, inside_x,
                                                 inside_y, 0, 0, 255, 255);
            surface->outside_ok =
                CheckColorPixel(bytes, row_bytes, 0, 0, 0, 0, 0, 255);
        }
        const bool ok = surface->result.succeeded() && surface->gpu_completed &&
                        surface->observed_readback && surface->inside_ok &&
                        surface->outside_ok;
        LogDrawResult(surface->result, feature_level, device_hresult,
                      surface->gpu_completed, surface->observed_readback,
                      surface->inside_ok, surface->outside_ok);
        return ok;
    } catch (Exception ^) {
        surface->result.status = xemu::D3D11DrawStatus::DeviceError;
        surface->result.hresult = E_FAIL;
        LogDrawResult(surface->result, feature_level, device_hresult,
                      surface->gpu_completed, surface->observed_readback,
                      surface->inside_ok, surface->outside_ok);
        return false;
    } catch (const std::exception &) {
        surface->result.status = xemu::D3D11DrawStatus::DeviceError;
        surface->result.hresult = E_FAIL;
        LogDrawResult(surface->result, feature_level, device_hresult,
                      surface->gpu_completed, surface->observed_readback,
                      surface->inside_ok, surface->outside_ok);
        return false;
    }
}

bool ProbeD3D11Clear(ID3D11Device *device, ID3D11DeviceContext *context)
{
    if (device == nullptr || context == nullptr) {
        Log("d3d11_clear_matrix", false, "device or context unavailable");
        return false;
    }
    xemu::D3D11ClearPrimitive clear(device, context);
    bool all_ok = clear.has_context1();
    Log("d3d11_clear_context1", clear.has_context1(),
        clear.has_context1() ? "D3D11.1 ClearView available" :
                               "D3D11.1 context unavailable");
    if (!clear.has_context1()) {
        Log("d3d11_clear_matrix", false,
            "rectangular color clear requires D3D11.1");
        return false;
    }

    constexpr UINT width = 8;
    constexpr UINT height = 6;
    const xemu::D3D11ClearRect full_rect = { 0, 0, width - 1, height - 1 };

    ComPtr<ID3D11Texture2D> rgba_texture;
    ComPtr<ID3D11RenderTargetView> rgba_view;
    bool rgba_ok =
        CreateColorTarget(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM,
                          &rgba_texture, &rgba_view);
    xemu::D3D11ClearParams color = {};
    color.surface_kind = xemu::D3D11ClearSurfaceKind::Color;
    color.surface_format = xemu::D3D11ClearSurfaceFormat::ColorRgba8Unorm;
    color.rect = full_rect;
    color.surface_width = width;
    color.surface_height = height;
    color.color[0] = 0.2f;
    color.color[1] = 0.4f;
    color.color[2] = 0.6f;
    color.color[3] = 0.8f;
    xemu::D3D11ClearResult result = {};
    if (rgba_ok) {
        result = clear.Clear(color, rgba_view.Get(), nullptr);
    }
    std::vector<uint8_t> bytes;
    UINT row_bytes = 0;
    const bool rgba_readback =
        rgba_ok &&
        ReadbackTexture(device, context, rgba_texture.Get(), 4, &bytes,
                        &row_bytes) &&
        CheckColorPixel(bytes, row_bytes, 0, 0, 51, 102, 153, 204);
    rgba_ok = rgba_ok && result.status == xemu::D3D11ClearStatus::Cleared &&
              rgba_readback;
    LogClearResult("d3d11_clear_color_rgba_full", result,
                   xemu::D3D11ClearStatus::Cleared, rgba_ok);
    all_ok = all_ok && rgba_ok;

    color.color[0] = 0.0f;
    color.color[1] = 0.0f;
    color.color[2] = 0.0f;
    color.color[3] = 1.0f;
    color.rect = { 1, 2, 3, 4 };
    result = clear.Clear(color, rgba_view.Get(), nullptr);
    bool rect_ok = result.status == xemu::D3D11ClearStatus::Cleared &&
                   ReadbackTexture(device, context, rgba_texture.Get(), 4,
                                   &bytes, &row_bytes);
    if (rect_ok) {
        for (UINT y = 0; y < height && rect_ok; ++y) {
            for (UINT x = 0; x < width; ++x) {
                const bool in_rect = x >= 1 && x <= 3 && y >= 2 && y <= 4;
                const bool pixel_ok =
                    in_rect ?
                        CheckColorPixel(bytes, row_bytes, x, y, 0, 0, 0, 255) :
                        CheckColorPixel(bytes, row_bytes, x, y, 51, 102, 153,
                                        204);
                if (!pixel_ok) {
                    rect_ok = false;
                    break;
                }
            }
        }
    }
    LogClearResult("d3d11_clear_color_rgba_rect", result,
                   xemu::D3D11ClearStatus::Cleared, rect_ok);
    all_ok = all_ok && rect_ok;

    ComPtr<ID3D11Texture2D> bgra_texture;
    ComPtr<ID3D11RenderTargetView> bgra_view;
    bool bgra_ok =
        CreateColorTarget(device, width, height, DXGI_FORMAT_B8G8R8A8_UNORM,
                          &bgra_texture, &bgra_view);
    color.surface_format = xemu::D3D11ClearSurfaceFormat::ColorBgra8Unorm;
    color.rect = full_rect;
    result = bgra_ok ? clear.Clear(color, bgra_view.Get(), nullptr) :
                       xemu::D3D11ClearResult{};
    bgra_ok = bgra_ok && result.status == xemu::D3D11ClearStatus::Cleared &&
              ReadbackTexture(device, context, bgra_texture.Get(), 4, &bytes,
                              &row_bytes) &&
              CheckColorPixel(bytes, row_bytes, 0, 0, 153, 102, 51, 255);
    LogClearResult("d3d11_clear_color_bgra_full", result,
                   xemu::D3D11ClearStatus::Cleared, bgra_ok);
    all_ok = all_ok && bgra_ok;

    ComPtr<ID3D11Texture2D> z16_texture;
    ComPtr<ID3D11DepthStencilView> z16_view;
    bool z16_ok =
        CreateDepthTarget(device, width, height, DXGI_FORMAT_D16_UNORM,
                          DXGI_FORMAT_D16_UNORM, &z16_texture, &z16_view);
    xemu::D3D11ClearParams z16 = {};
    z16.surface_kind = xemu::D3D11ClearSurfaceKind::DepthStencil;
    z16.surface_format = xemu::D3D11ClearSurfaceFormat::Z16Fixed;
    z16.rect = full_rect;
    z16.surface_width = width;
    z16.surface_height = height;
    z16.fixed_depth = 0x8000;
    z16.clear_depth = true;
    z16.clear_stencil = false;
    result = z16_ok ? clear.Clear(z16, nullptr, z16_view.Get()) :
                      xemu::D3D11ClearResult{};
    z16_ok = z16_ok && result.status == xemu::D3D11ClearStatus::Cleared &&
             ReadbackTexture(device, context, z16_texture.Get(), 2, &bytes,
                             &row_bytes) &&
             bytes.size() >= 2 && bytes[0] == 0 && bytes[1] == 0x80;
    LogClearResult("d3d11_clear_z16_fixed_full", result,
                   xemu::D3D11ClearStatus::Cleared, z16_ok);
    all_ok = all_ok && z16_ok;

    ComPtr<ID3D11Texture2D> z24_texture;
    ComPtr<ID3D11DepthStencilView> z24_view;
    bool z24_ok = CreateDepthTarget(
        device, width, height, DXGI_FORMAT_R24G8_TYPELESS,
        DXGI_FORMAT_D24_UNORM_S8_UINT, &z24_texture, &z24_view);
    xemu::D3D11ClearParams z24 = {};
    z24.surface_kind = xemu::D3D11ClearSurfaceKind::DepthStencil;
    z24.surface_format = xemu::D3D11ClearSurfaceFormat::Z24S8Fixed;
    z24.rect = full_rect;
    z24.surface_width = width;
    z24.surface_height = height;
    z24.fixed_depth = 0x102030;
    z24.stencil = 0x5a;
    z24.clear_depth = true;
    z24.clear_stencil = true;
    if (z24_ok) {
        context->ClearDepthStencilView(z24_view.Get(),
                                       D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
                                       0.25f, 0x5a);
        result = clear.Clear(z24, nullptr, z24_view.Get());
    }
    z24_ok =
        z24_ok && result.status == xemu::D3D11ClearStatus::Cleared &&
        ReadbackTexture(device, context, z24_texture.Get(), 4, &bytes,
                        &row_bytes) &&
        CheckDepthStencilPixel(bytes, row_bytes, 0, 0, 0x102030, 0x5a, true);
    LogClearResult("d3d11_clear_z24s8_depth_stencil", result,
                   xemu::D3D11ClearStatus::Cleared, z24_ok);
    all_ok = all_ok && z24_ok;

    z24.fixed_depth = 0x203040;
    z24.stencil = 0x11;
    z24.clear_depth = true;
    z24.clear_stencil = false;
    result = z24_ok ? clear.Clear(z24, nullptr, z24_view.Get()) :
                      xemu::D3D11ClearResult{};
    bool depth_only_ok =
        z24_ok && result.status == xemu::D3D11ClearStatus::Cleared &&
        ReadbackTexture(device, context, z24_texture.Get(), 4, &bytes,
                        &row_bytes) &&
        CheckDepthStencilPixel(bytes, row_bytes, 0, 0, 0x203040, 0x5a, true);
    LogClearResult("d3d11_clear_z24s8_depth_only", result,
                   xemu::D3D11ClearStatus::Cleared, depth_only_ok);
    all_ok = all_ok && depth_only_ok;

    z24.fixed_depth = UINT32_MAX;
    z24.stencil = 0x11;
    z24.clear_depth = false;
    z24.clear_stencil = true;
    result = z24_ok ? clear.Clear(z24, nullptr, z24_view.Get()) :
                      xemu::D3D11ClearResult{};
    bool stencil_only_ok =
        z24_ok && result.status == xemu::D3D11ClearStatus::Cleared &&
        ReadbackTexture(device, context, z24_texture.Get(), 4, &bytes,
                        &row_bytes) &&
        CheckDepthStencilPixel(bytes, row_bytes, 0, 0, 0x203040, 0x11, true);
    LogClearResult("d3d11_clear_z24s8_stencil_only", result,
                   xemu::D3D11ClearStatus::Cleared, stencil_only_ok);
    all_ok = all_ok && stencil_only_ok;

    ComPtr<ID3D11Texture2D> unsupported_texture;
    ComPtr<ID3D11RenderTargetView> unsupported_view;
    bool unsupported_target =
        CreateColorTarget(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM,
                          &unsupported_texture, &unsupported_view);
    xemu::D3D11ClearParams unsupported = color;
    unsupported.surface_format = xemu::D3D11ClearSurfaceFormat::ColorRgba8Unorm;
    unsupported.color_mask = xemu::D3D11_CLEAR_RED;
    result = unsupported_target ?
                 clear.Clear(unsupported, unsupported_view.Get(), nullptr) :
                 xemu::D3D11ClearResult{};
    bool mask_ok = unsupported_target &&
                   result.status == xemu::D3D11ClearStatus::Unsupported;
    LogClearResult("d3d11_clear_unsupported_partial_mask", result,
                   xemu::D3D11ClearStatus::Unsupported, mask_ok);
    all_ok = all_ok && mask_ok;

    unsupported.surface_kind = xemu::D3D11ClearSurfaceKind::DepthStencil;
    unsupported.surface_format = xemu::D3D11ClearSurfaceFormat::Z16Float;
    unsupported.color_mask = xemu::D3D11_CLEAR_ALL_COLOR_CHANNELS;
    unsupported.clear_depth = true;
    unsupported.clear_stencil = false;
    result = clear.Clear(unsupported, nullptr, nullptr);
    const bool float_ok = result.status == xemu::D3D11ClearStatus::Unsupported;
    LogClearResult("d3d11_clear_unsupported_float_depth", result,
                   xemu::D3D11ClearStatus::Unsupported, float_ok);
    all_ok = all_ok && float_ok;

    unsupported.surface_kind = xemu::D3D11ClearSurfaceKind::Color;
    unsupported.surface_format = xemu::D3D11ClearSurfaceFormat::ColorRgba8Unorm;
    unsupported.swizzled = true;
    result = unsupported_target ?
                 clear.Clear(unsupported, unsupported_view.Get(), nullptr) :
                 xemu::D3D11ClearResult{};
    const bool swizzled_ok =
        unsupported_target &&
        result.status == xemu::D3D11ClearStatus::Unsupported;
    LogClearResult("d3d11_clear_unsupported_swizzled", result,
                   xemu::D3D11ClearStatus::Unsupported, swizzled_ok);
    all_ok = all_ok && swizzled_ok;

    Log("d3d11_clear_matrix", all_ok,
        all_ok ?
            "RGBA/BGRA, rectangle, Z16, Z24S8 and unsupported cases passed" :
            "one or more D3D11 clear cases failed");
    return all_ok;
}

} // namespace

namespace xemu_uwp_host_probe {
[Windows::Foundation::Metadata::WebHostHidden] public ref class ProbeView sealed
    : IFrameworkView {
    Agile<CoreWindow> window_;
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;

    static void PatternPixel(UINT x, UINT y, uint8_t *pixel)
    {
        pixel[0] = static_cast<uint8_t>(x & 0xffu);
        pixel[1] = static_cast<uint8_t>(y & 0xffu);
        pixel[2] = static_cast<uint8_t>((x * 17u + y * 31u + 0x42u) & 0xffu);
        pixel[3] = 0xff;
    }

    static bool PixelMatches(const uint8_t *actual, UINT x, UINT y)
    {
        uint8_t expected[4] = {};
        PatternPixel(x, y, expected);
        return memcmp(actual, expected, sizeof(expected)) == 0;
    }

    static void PixelText(const uint8_t *pixel, char *text, size_t textSize)
    {
        sprintf_s(text, textSize, "#%02x%02x%02x%02x", pixel[2], pixel[1],
                  pixel[0], pixel[3]);
    }

    bool WaitForGpuCompletion()
    {
        if (!device_ || !context_) {
            Log("d3d11_gpu_completion", false, "device or context unavailable");
            return false;
        }

        D3D11_QUERY_DESC queryDesc = {};
        queryDesc.Query = D3D11_QUERY_EVENT;
        ComPtr<ID3D11Query> query;
        HRESULT hr = device_->CreateQuery(&queryDesc, &query);
        if (FAILED(hr)) {
            char detail[96] = {};
            sprintf_s(detail, "D3D11_QUERY_EVENT creation failed (0x%08lx)",
                      static_cast<unsigned long>(hr));
            Log("d3d11_gpu_completion", false, detail);
            return false;
        }

        context_->End(query.Get());
        context_->Flush();
        constexpr DWORD timeoutMs = 5000;
        const ULONGLONG deadline = GetTickCount64() + timeoutMs;
        for (;;) {
            const HRESULT dataHr = context_->GetData(
                query.Get(), nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH);
            if (dataHr == S_OK) {
                Log("d3d11_gpu_completion", true,
                    "D3D11_QUERY_EVENT completed after Flush");
                return true;
            }
            if (FAILED(dataHr)) {
                char detail[96] = {};
                sprintf_s(detail, "D3D11_QUERY_EVENT GetData failed (0x%08lx)",
                          static_cast<unsigned long>(dataHr));
                Log("d3d11_gpu_completion", false, detail);
                return false;
            }
            if (GetTickCount64() >= deadline) {
                Log("d3d11_gpu_completion", false,
                    "D3D11_QUERY_EVENT timed out after 5000 ms");
                return false;
            }
            Sleep(1);
        }
    }

    bool ReadbackPixels(const ID3D11Texture2D *backBuffer, UINT width,
                        UINT height)
    {
        if (!backBuffer || width == 0 || height == 0)
            return false;
        if (width > std::numeric_limits<size_t>::max() / 4u) {
            Log("d3d11_readback", false, "width*4 overflows size_t");
            return false;
        }
        const size_t rowBytes = static_cast<size_t>(width) * 4u;

        // The presenter exposes its back buffer as a borrowed diagnostic view.
        // D3D11's C++ interface is not const-qualified, so retain the read-only
        // ownership contract while using the API's const-agnostic methods.
        ID3D11Texture2D *readbackSource =
            const_cast<ID3D11Texture2D *>(backBuffer);

        D3D11_TEXTURE2D_DESC stagingDesc = {};
        readbackSource->GetDesc(&stagingDesc);
        stagingDesc.Usage = D3D11_USAGE_STAGING;
        stagingDesc.BindFlags = 0;
        stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        stagingDesc.MiscFlags = 0;

        ComPtr<ID3D11Texture2D> staging;
        if (FAILED(device_->CreateTexture2D(&stagingDesc, nullptr, &staging))) {
            Log("d3d11_readback", false, "staging texture creation failed");
            return false;
        }
        context_->CopyResource(staging.Get(), readbackSource);
        if (!WaitForGpuCompletion()) {
            Log("d3d11_readback", false,
                "GPU completion was not confirmed after staging CopyResource");
            return false;
        }

        D3D11_MAPPED_SUBRESOURCE mapped = {};
        const HRESULT mapHr =
            context_->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(mapHr)) {
            Log("d3d11_readback", false, "staging texture map failed");
            return false;
        }
        if (!mapped.pData || mapped.RowPitch < rowBytes) {
            context_->Unmap(staging.Get(), 0);
            Log("d3d11_readback", false, "staging texture row pitch invalid");
            return false;
        }

        const UINT points[][2] = {
            { 0, 0 },
            { width / 2, height / 2 },
            { width - 1, height - 1 },
        };
        bool allOk = true;
        for (size_t i = 0; i < _countof(points); ++i) {
            const UINT x = points[i][0];
            const UINT y = points[i][1];
            const auto *pixel = static_cast<const uint8_t *>(mapped.pData) +
                                static_cast<size_t>(y) * mapped.RowPitch +
                                static_cast<size_t>(x) * 4u;
            uint8_t expectedPixel[4] = {};
            PatternPixel(x, y, expectedPixel);
            char expected[32] = {};
            char actual[32] = {};
            PixelText(expectedPixel, expected, sizeof(expected));
            PixelText(pixel, actual, sizeof(actual));
            const bool ok = PixelMatches(pixel, x, y);
            char name[64] = {};
            sprintf_s(name, "d3d11_readback_pixel_%zu", i);
            LogPixel(name, ok, x, y, expected, actual);
            allOk = allOk && ok;
        }
        context_->Unmap(staging.Get(), 0);
        Log("d3d11_readback", allOk,
            allOk ? "representative pixels match" :
                    "representative pixel mismatch");
        return allOk;
    }

    bool ReadbackDrawPixels(const ID3D11Texture2D *backBuffer, UINT width,
                            UINT height)
    {
        if (backBuffer == nullptr || width < 4 || height < 4 ||
            !WaitForGpuCompletion()) {
            return false;
        }
        std::vector<uint8_t> bytes;
        UINT row_bytes = 0;
        if (!ReadbackTexture(device_.Get(), context_.Get(),
                             const_cast<ID3D11Texture2D *>(backBuffer), 4,
                             &bytes, &row_bytes)) {
            return false;
        }
        return CheckColorPixel(bytes, row_bytes, width / 2, height / 2, 0, 0,
                               255, 255) &&
               CheckColorPixel(bytes, row_bytes, 0, 0, 0, 0, 0, 255);
    }

public:
    virtual void Initialize(CoreApplicationView ^ applicationView)
    {
        (void)applicationView;
    }
    virtual void SetWindow(CoreWindow ^ window)
    {
        window_ = window;
    }
    virtual void Load(String ^ entryPoint)
    {
        (void)entryPoint;
    }
    virtual void Uninitialize()
    {
    }
    virtual void Run()
    {
        ProbeStorage();
        ProbeCodeGeneration();
        ProbeAddressSpace();
        ProbeThreadsAndTls();
        ProbeGamepads();

        CoreWindow ^ window = window_.Get();
        if (window)
            window->Activate();

        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        D3D_FEATURE_LEVEL level = static_cast<D3D_FEATURE_LEVEL>(0);
        HRESULT hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0,
            D3D11_SDK_VERSION, &device_, &level, &context_);
        bool deviceOk = SUCCEEDED(hr);
        Log("d3d11_device", deviceOk,
            deviceOk ? "hardware device created" : "D3D11CreateDevice failed");
        const bool clearOk =
            deviceOk && ProbeD3D11Clear(device_.Get(), context_.Get());
        if (!deviceOk) {
            Log("d3d11_clear_matrix", false,
                "skipped because device creation failed");
        }

        UINT drawWidth = 64;
        UINT drawHeight = 64;
        if (window) {
            const Windows::Foundation::Rect bounds = window->Bounds;
            drawWidth = std::max(1u, static_cast<UINT>(bounds.Width + 0.5f));
            drawHeight = std::max(1u, static_cast<UINT>(bounds.Height + 0.5f));
        }
        DrawProbeSurface drawSurface;
        bool drawOk =
            deviceOk && ProbeD3D11Draw(device_.Get(), context_.Get(), level, hr,
                                       drawWidth, drawHeight, &drawSurface);
        if (!deviceOk) {
            xemu::D3D11DrawResult drawResult = {};
            drawResult.hresult = hr;
            LogDrawResult(drawResult, level, hr, false, false, false, false);
        }

        bool presentOk = false;
        bool uploadOk = false;
        bool copyOk = false;
        bool readbackOk = false;
        bool drawPresentedOk = false;
        if (deviceOk && window) {
            const Windows::Foundation::Rect bounds = window->Bounds;
            const UINT requestedWidth =
                std::max(1u, static_cast<UINT>(bounds.Width + 0.5f));
            const UINT requestedHeight =
                std::max(1u, static_cast<UINT>(bounds.Height + 0.5f));
            xemu::D3D11CoreWindowPresenter presenter(device_.Get(),
                                                     context_.Get());
            IUnknown *coreWindowUnknown = reinterpret_cast<IUnknown *>(window);
            const bool swapChainOk = presenter.Initialize(
                coreWindowUnknown, requestedWidth, requestedHeight);
            Log("d3d11_swap_chain", swapChainOk,
                swapChainOk ? "CoreWindow swap chain created" :
                              "CoreWindow swap chain creation failed");
            if (swapChainOk) {
                const UINT width = presenter.width();
                const UINT height = presenter.height();
                std::vector<uint8_t> pattern;
                bool validPatternSize =
                    width <= std::numeric_limits<size_t>::max() / 4u &&
                    height <=
                        (std::numeric_limits<size_t>::max() / 4u) / width &&
                    width <= std::numeric_limits<UINT>::max() / 4u;
                const UINT rowBytes = validPatternSize ? width * 4u : 0;
                if (validPatternSize) {
                    pattern.resize(static_cast<size_t>(width) * height * 4u);
                    for (UINT y = 0; y < height; ++y) {
                        for (UINT x = 0; x < width; ++x) {
                            PatternPixel(
                                x, y,
                                pattern.data() +
                                    (static_cast<size_t>(y) * width + x) * 4u);
                        }
                    }
                }

                xemu::D3D11FrameUploader uploader(device_.Get(),
                                                  context_.Get());
                xemu::D3D11FrameView frame;
                frame.data = pattern.data();
                frame.width = width;
                frame.height = height;
                frame.stride = rowBytes;
                frame.format = xemu::D3D11FrameFormat::BGRA8_UNORM;
                frame.orientation = xemu::D3D11FrameOrientation::TopDown;
                uploadOk = validPatternSize && uploader.Resize(width, height) &&
                           uploader.Upload(frame);
                Log("d3d11_frame_upload", uploadOk,
                    uploadOk ? "D3D11FrameUploader uploaded BGRA pattern" :
                               "D3D11FrameUploader upload failed");
                if (drawOk &&
                    presenter.IsTextureCompatible(drawSurface.texture.Get())) {
                    copyOk = presenter.CopyTexture(drawSurface.texture.Get());
                    Log("d3d11_copy_resource", copyOk,
                        copyOk ? "indexed draw surface copied to CoreWindow "
                                 "backbuffer" :
                                 "draw surface copy to backbuffer failed");
                    if (copyOk) {
                        readbackOk = ReadbackDrawPixels(presenter.back_buffer(),
                                                        width, height);
                        presentOk = presenter.Present(1, 0);
                        drawPresentedOk = readbackOk && presentOk;
                        if (!presentOk) {
                            char detail[160] = {};
                            sprintf_s(
                                detail, "Present returned HRESULT 0x%08lx%s",
                                static_cast<unsigned long>(
                                    presenter.last_present_hresult()),
                                presenter.device_lost() ? " (device lost)" :
                                                          "");
                            Log("d3d11_present_status", false, detail);
                        }
                    }
                } else if (uploadOk) {
                    copyOk = presenter.CopyTexture(uploader.texture());
                    Log("d3d11_copy_resource", copyOk,
                        copyOk ?
                            "shader-free compatible BGRA texture copy" :
                            "backbuffer and uploaded texture are incompatible");
                    if (copyOk) {
                        // Read the copied flip-model backbuffer before
                        // Present(). Present may rotate the buffer, making a
                        // post-present readback observe a different surface
                        // than the one copied.
                        readbackOk = ReadbackPixels(presenter.back_buffer(),
                                                    width, height);
                        presentOk = presenter.Present(1, 0);
                        if (!presentOk) {
                            char detail[160] = {};
                            sprintf_s(
                                detail, "Present returned HRESULT 0x%08lx%s",
                                static_cast<unsigned long>(
                                    presenter.last_present_hresult()),
                                presenter.device_lost() ? " (device lost)" :
                                                          "");
                            Log("d3d11_present_status", false, detail);
                        }
                    }
                }
            }
        }
        Log("d3d11_present", presentOk,
            presentOk ? "CoreWindow swap chain presented" :
                        "CoreWindow swap chain unavailable");
        const bool framePathOk =
            (drawOk || uploadOk) && copyOk && presentOk && readbackOk;
        Log("d3d11_frame_path", framePathOk,
            framePathOk ? "draw/upload, copy, present, and readback passed" :
                          "one or more D3D11 frame-path checks failed");
        Log("d3d11_windows_path", clearOk && drawPresentedOk,
            clearOk && drawPresentedOk ?
                "D3D11 clear, indexed draw, copy, present, and readback "
                "passed" :
                "one or more D3D11 Windows-path checks failed");
        if (window) {
            window->Dispatcher->ProcessEvents(
                CoreProcessEventsOption::ProcessUntilQuit);
        }
    }
};

[Windows::Foundation::Metadata::WebHostHidden] public ref class ProbeSource
    sealed : IFrameworkViewSource {
public:
    virtual IFrameworkView ^ CreateView()
    {
        return ref new ProbeView();
    }
};
} // namespace xemu_uwp_host_probe

[MTAThread] int main(Platform::Array<Platform::String ^> ^) {
    Microsoft::WRL::Wrappers::RoInitializeWrapper initialize(
        RO_INIT_MULTITHREADED);
    if (FAILED(initialize))
        return 1;
    CoreApplication::Run(ref new xemu_uwp_host_probe::ProbeSource());
    return 0;
}
