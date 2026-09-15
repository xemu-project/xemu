/* One-pixel WARP parity check for the offline PSH interpreter. */

#include "hw/xbox/nv2a/pgraph/d3d11/psh_interpreter.h"

#ifdef _WIN32

#include "qemu/osdep.h"

#include <d3d11.h>

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace {

struct Vertex {
    float position[3];
    float v0[4], v1[4];
    float t0[4], t1[4], t2[4], t3[4], fog[4];
};

struct TestCase {
    PshReferenceProgram program;
    PshReferenceInputs input;
};

static uint32_t Pack(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    return a << 24 | b << 16 | c << 8 | d;
}

static bool Run(void)
{
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                                 nullptr, 0, D3D11_SDK_VERSION,
                                 device.ReleaseAndGetAddressOf(), &level,
                                 context.ReleaseAndGetAddressOf()))) {
        return false;
    }
    const xemu::d3d11_psh::ShaderBytecode shaders =
        xemu::d3d11_psh::GetShaderBytecode();

    PshReferenceProgram program = {};
    PshReferenceInputs input = {};
    PshReferenceResult expected = {};
    char error[128];
    program.combiner_control = 1;
    program.shader_stage_program = PS_TEXTUREMODES_PASSTHRU;
    program.rgb_inputs[0] =
        Pack(PS_REGISTER_T0, PS_REGISTER_C0, PS_REGISTER_V1, PS_REGISTER_V0);
    program.alpha_inputs[0] =
        Pack((uint32_t)PS_REGISTER_T0 | (uint32_t)PS_CHANNEL_ALPHA,
             (uint32_t)PS_REGISTER_C1 | (uint32_t)PS_CHANNEL_ALPHA,
             (uint32_t)PS_REGISTER_V1 | (uint32_t)PS_CHANNEL_ALPHA,
             (uint32_t)PS_REGISTER_V0 | (uint32_t)PS_CHANNEL_ALPHA);
    program.rgb_outputs[0] = (PS_REGISTER_V0 << 4) | PS_REGISTER_ZERO;
    program.alpha_outputs[0] = (PS_REGISTER_V0 << 4) | PS_REGISTER_ZERO;
    input.v0 = { .2f, .3f, .4f, .5f };
    input.v1 = { .7f, .6f, .5f, .4f };
    input.t[0] = { .1f, .2f, .3f, .4f };
    input.c0[0] = { .9f, .8f, .7f, .6f };
    input.c1[0] = { .6f, .5f, .4f, .3f };

    TestCase cases[4] = { { program, input }, {}, {}, {} };
    cases[1].program.combiner_control = 1;
    cases[1].program.shader_stage_program = PS_TEXTUREMODES_NONE;
    cases[1].program.rgb_inputs[0] = Pack(PS_REGISTER_T0, PS_REGISTER_ONE,
                                          PS_REGISTER_ZERO, PS_REGISTER_ZERO);
    cases[1].program.alpha_inputs[0] =
        Pack((uint32_t)PS_REGISTER_T0 | (uint32_t)PS_CHANNEL_ALPHA,
             PS_REGISTER_ONE, PS_REGISTER_ZERO, PS_REGISTER_ZERO);
    cases[1].program.rgb_outputs[0] = PS_REGISTER_V0 << 4;
    cases[1].program.alpha_outputs[0] = PS_REGISTER_V0 << 4;
    cases[1].input = input;
    cases[1].input.t[0] = { .9f, .8f, .7f, .25f };

    cases[2].program.combiner_control =
        2 | ((PS_COMBINERCOUNT_MUX_MSB | PS_COMBINERCOUNT_UNIQUE_C0 |
              PS_COMBINERCOUNT_UNIQUE_C1)
             << 8);
    /* The third mode is a deliberate decoy: it is semantically unused by
     * this two-stage program and must not be treated as combiner flags. */
    cases[2].program.shader_stage_program = PS_TEXTUREMODES_PASSTHRU |
                                            (PS_TEXTUREMODES_PASSTHRU << 5) |
                                            (PS_TEXTUREMODES_PASSTHRU << 10);
    cases[2].program.rgb_inputs[0] =
        Pack(PS_REGISTER_C0, PS_REGISTER_C1, PS_REGISTER_V1, PS_REGISTER_ONE);
    cases[2].program.alpha_inputs[0] =
        Pack((uint32_t)PS_REGISTER_T0 | (uint32_t)PS_CHANNEL_ALPHA,
             PS_REGISTER_ONE, PS_REGISTER_ZERO, PS_REGISTER_ZERO);
    cases[2].program.rgb_outputs[0] = PS_REGISTER_R0 << 4;
    cases[2].program.alpha_outputs[0] = PS_REGISTER_R0 << 4;
    cases[2].program.rgb_inputs[1] =
        Pack(PS_REGISTER_C0, PS_REGISTER_C1, PS_REGISTER_R0, PS_REGISTER_ONE);
    cases[2].program.alpha_inputs[1] = Pack(
        (uint32_t)PS_REGISTER_C0 | (uint32_t)PS_CHANNEL_ALPHA,
        (uint32_t)PS_REGISTER_C1 | (uint32_t)PS_CHANNEL_ALPHA,
        (uint32_t)PS_REGISTER_R0 | (uint32_t)PS_CHANNEL_ALPHA, PS_REGISTER_ONE);
    cases[2].program.rgb_outputs[1] = PS_REGISTER_V0 << 8;
    cases[2].program.alpha_outputs[1] =
        (PS_REGISTER_V0 << 8) | (PS_COMBINEROUTPUT_AB_CD_MUX << 12);
    cases[2].input = input;
    /* 128/255 is above the MSB threshold but quantizes to an even byte, so
     * MSB and LSB modes select different alpha operands. */
    cases[2].input.t[0].w = 128.0f / 255.0f;
    cases[2].input.c0[0] = { .2f, .2f, .2f, .2f };
    cases[2].input.c1[0] = { .3f, .3f, .3f, .3f };
    cases[2].input.c0[1] = { .4f, .4f, .4f, .4f };
    cases[2].input.c1[1] = { .5f, .5f, .5f, .5f };

    cases[3].program.combiner_control = 1;
    cases[3].program.shader_stage_program = PS_TEXTUREMODES_PASSTHRU;
    cases[3].program.rgb_inputs[0] =
        Pack(PS_REGISTER_V0, PS_REGISTER_V1, PS_REGISTER_ONE, PS_REGISTER_ONE);
    cases[3].program.alpha_inputs[0] = cases[0].program.alpha_inputs[0];
    cases[3].program.rgb_outputs[0] =
        (PS_REGISTER_R0 << 4) |
        ((PS_COMBINEROUTPUT_AB_DOT_PRODUCT | PS_COMBINEROUTPUT_AB_BLUE_TO_ALPHA)
         << 12);
    cases[3].program.alpha_outputs[0] = PS_REGISTER_R0 << 4;
    cases[3].program.final_inputs_0 = Pack(PS_REGISTER_R0, PS_REGISTER_ONE,
                                           PS_REGISTER_ZERO, PS_REGISTER_ZERO);
    cases[3].program.final_inputs_1 =
        Pack(PS_REGISTER_V0, PS_REGISTER_V0,
             (uint32_t)PS_REGISTER_V0 | (uint32_t)PS_CHANNEL_ALPHA, 0);
    cases[3].input = input;

    xemu::d3d11_psh::InterpreterConstants constants = {};
    if (!xemu::d3d11_psh::BuildConstants(program, input, &constants, error,
                                         sizeof(error))) {
        return false;
    }
    D3D11_BUFFER_DESC cb_desc = {};
    cb_desc.ByteWidth = sizeof(constants);
    cb_desc.Usage = D3D11_USAGE_DEFAULT;
    cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SUBRESOURCE_DATA cb_data = { &constants, 0, 0 };
    ComPtr<ID3D11Buffer> constant_buffer;
    if (FAILED(device->CreateBuffer(&cb_desc, &cb_data,
                                    constant_buffer.GetAddressOf()))) {
        return false;
    }

    Vertex vertices[3] = {};
    const float positions[3][3] = { { -1, -1, 0 }, { -1, 3, 0 }, { 3, -1, 0 } };
    for (unsigned i = 0; i < 3; ++i) {
        memcpy(vertices[i].position, positions[i],
               sizeof(vertices[i].position));
        memcpy(vertices[i].v0, &input.v0, sizeof(vertices[i].v0));
        memcpy(vertices[i].v1, &input.v1, sizeof(vertices[i].v1));
        memcpy(vertices[i].t0, &input.t[0], sizeof(vertices[i].t0));
        memcpy(vertices[i].fog, &input.fog, sizeof(vertices[i].fog));
    }
    D3D11_BUFFER_DESC vb_desc = {};
    vb_desc.ByteWidth = sizeof(vertices);
    vb_desc.Usage = D3D11_USAGE_DEFAULT;
    vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vb_data = { vertices, 0, 0 };
    ComPtr<ID3D11Buffer> vertex_buffer;
    if (FAILED(device->CreateBuffer(&vb_desc, &vb_data,
                                    vertex_buffer.GetAddressOf()))) {
        return false;
    }

    D3D11_TEXTURE2D_DESC texture_desc = {};
    texture_desc.Width = texture_desc.Height = 1;
    texture_desc.MipLevels = texture_desc.ArraySize = 1;
    texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> texture;
    if (FAILED(device->CreateTexture2D(&texture_desc, nullptr,
                                       texture.GetAddressOf()))) {
        return false;
    }
    ComPtr<ID3D11RenderTargetView> rtv;
    if (FAILED(device->CreateRenderTargetView(texture.Get(), nullptr,
                                              rtv.GetAddressOf()))) {
        return false;
    }
    texture_desc.Usage = D3D11_USAGE_STAGING;
    texture_desc.BindFlags = 0;
    texture_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(device->CreateTexture2D(&texture_desc, nullptr,
                                       staging.GetAddressOf()))) {
        return false;
    }

    static const D3D11_INPUT_ELEMENT_DESC elements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
          offsetof(Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, v0),
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, v1),
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
          offsetof(Vertex, t0), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
          offsetof(Vertex, t1), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
          offsetof(Vertex, t2), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
          offsetof(Vertex, t3), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 4, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
          offsetof(Vertex, fog), D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    ComPtr<ID3D11VertexShader> vertex_shader;
    ComPtr<ID3D11PixelShader> pixel_shader;
    ComPtr<ID3D11InputLayout> layout;
    if (FAILED(device->CreateVertexShader(shaders.vertex_shader,
                                          shaders.vertex_shader_size, nullptr,
                                          vertex_shader.GetAddressOf())) ||
        FAILED(device->CreatePixelShader(shaders.pixel_shader,
                                         shaders.pixel_shader_size, nullptr,
                                         pixel_shader.GetAddressOf())) ||
        FAILED(device->CreateInputLayout(
            elements, ARRAYSIZE(elements), shaders.vertex_shader,
            shaders.vertex_shader_size, layout.GetAddressOf()))) {
        return false;
    }

    D3D11_VIEWPORT viewport = { 0, 0, 1, 1, 0, 1 };
    UINT stride = sizeof(Vertex), offset = 0;
    context->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);
    context->RSSetViewports(1, &viewport);
    context->IASetInputLayout(layout.Get());
    context->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &stride,
                                &offset);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertex_shader.Get(), nullptr, 0);
    context->PSSetShader(pixel_shader.Get(), nullptr, 0);
    context->PSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());
    for (const TestCase &test_case : cases) {
        if (!psh_reference_evaluate(&test_case.program, &test_case.input,
                                    &expected, error, sizeof(error)) ||
            !xemu::d3d11_psh::BuildConstants(test_case.program, test_case.input,
                                             &constants, error,
                                             sizeof(error))) {
            return false;
        }
        context->UpdateSubresource(constant_buffer.Get(), 0, nullptr,
                                   &constants, 0, 0);
        for (unsigned i = 0; i < 3; ++i) {
            memcpy(vertices[i].v0, &test_case.input.v0, sizeof(vertices[i].v0));
            memcpy(vertices[i].v1, &test_case.input.v1, sizeof(vertices[i].v1));
            memcpy(vertices[i].t0, &test_case.input.t[0],
                   sizeof(vertices[i].t0));
            memcpy(vertices[i].t1, &test_case.input.t[1],
                   sizeof(vertices[i].t1));
            memcpy(vertices[i].t2, &test_case.input.t[2],
                   sizeof(vertices[i].t2));
            memcpy(vertices[i].t3, &test_case.input.t[3],
                   sizeof(vertices[i].t3));
            memcpy(vertices[i].fog, &test_case.input.fog,
                   sizeof(vertices[i].fog));
        }
        context->UpdateSubresource(vertex_buffer.Get(), 0, nullptr, vertices, 0,
                                   0);
        const float clear[4] = {};
        context->ClearRenderTargetView(rtv.Get(), clear);
        context->Draw(3, 0);
        context->CopyResource(staging.Get(), texture.Get());
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (FAILED(
                context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
            return false;
        }
        const uint8_t *pixel = static_cast<const uint8_t *>(mapped.pData);
        for (unsigned i = 0; i < 4; ++i) {
            const float value = (&expected.color.x)[i];
            const unsigned quantized = static_cast<unsigned>(
                std::lround(fminf(fmaxf(value, 0.0f), 1.0f) * 255.0f));
            if (std::abs(static_cast<int>(pixel[i]) -
                         static_cast<int>(quantized)) > 2) {
                context->Unmap(staging.Get(), 0);
                return false;
            }
        }
        context->Unmap(staging.Get(), 0);
    }
    return true;
}

} // namespace

int main()
{
    return Run() ? 0 : 1;
}

#else

int main()
{
    return 0;
}

#endif
