#version 310 es
#extension GL_QCOM_image_processing3 : enable

precision mediump float;
precision highp usampler2D;
precision highp sampler2D;
precision highp isampler2DArray;

layout(location = 0) in vec2 c2D;
layout(location = 1) in vec3 c3D;
layout(location = 2) in flat ivec2 offset;

layout(location = 3) out vec4 FragColor;

layout(binding = 0) uniform highp usampler2D usamp2d;
layout(binding = 1) uniform highp isampler2D isamp2d;
layout(binding = 2) uniform highp  sampler2D fsamp2d;

layout(binding = 3) uniform highp usampler2DArray usamp2da;
layout(binding = 4) uniform highp isampler2DArray isamp2da;
layout(binding = 5) uniform highp  sampler2DArray fsamp2da;

void main()
{
    vec4 sum = vec4(0.0f);

    sum += vec4(textureGather4x1QCOM(usamp2d, c2D));
    sum += vec4(textureGather4x1QCOM(usamp2d, c2D, 0));
    sum += vec4(textureGather4x1QCOM(isamp2d, c2D));
    sum += vec4(textureGather4x1QCOM(isamp2d, c2D, 1));
    sum += vec4(textureGather4x1QCOM(fsamp2d, c2D));
    sum += vec4(textureGather4x1QCOM(fsamp2d, c2D, 2));

    sum += vec4(textureGatherV2QCOM(usamp2d, c2D));
    sum += vec4(textureGatherV2QCOM(usamp2d, c2D, 1));
    sum += vec4(textureGatherV2QCOM(isamp2d, c2D));
    sum += vec4(textureGatherV2QCOM(isamp2d, c2D, 2));
    sum += vec4(textureGatherV2QCOM(fsamp2d, c2D));
    sum += vec4(textureGatherV2QCOM(fsamp2d, c2D, 3));

    sum += vec4(textureGatherH2QCOM(usamp2d, c2D));
    sum += vec4(textureGatherH2QCOM(usamp2d, c2D, 0));
    sum += vec4(textureGatherH2QCOM(isamp2d, c2D));
    sum += vec4(textureGatherH2QCOM(isamp2d, c2D, 1));
    sum += vec4(textureGatherH2QCOM(fsamp2d, c2D));
    sum += vec4(textureGatherH2QCOM(fsamp2d, c2D, 2));

    sum += vec4(textureGatherDQCOM(usamp2d, c2D));
    sum += vec4(textureGatherDQCOM(usamp2d, c2D, 1));
    sum += vec4(textureGatherDQCOM(isamp2d, c2D));
    sum += vec4(textureGatherDQCOM(isamp2d, c2D, 2));
    sum += vec4(textureGatherDQCOM(fsamp2d, c2D));
    sum += vec4(textureGatherDQCOM(fsamp2d, c2D, 3));

    sum += vec4(textureGather4x1QCOM(usamp2da, c3D));
    sum += vec4(textureGather4x1QCOM(isamp2da, c3D));
    sum += vec4(textureGather4x1QCOM(fsamp2da, c3D));

    sum += vec4(textureGatherV2QCOM(usamp2da, c3D));
    sum += vec4(textureGatherV2QCOM(isamp2da, c3D));
    sum += vec4(textureGatherV2QCOM(fsamp2da, c3D));

    sum += vec4(textureGatherH2QCOM(usamp2da, c3D));
    sum += vec4(textureGatherH2QCOM(isamp2da, c3D));
    sum += vec4(textureGatherH2QCOM(fsamp2da, c3D));

    sum += vec4(textureGatherDQCOM(usamp2da, c3D));
    sum += vec4(textureGatherDQCOM(isamp2da, c3D));
    sum += vec4(textureGatherDQCOM(fsamp2da, c3D));

    // textureGather*OffsetQCOM
    sum += vec4(textureGather4x1OffsetQCOM(usamp2d, c2D, offset));
    sum += vec4(textureGather4x1OffsetQCOM(usamp2d, c2D, offset, 0));
    sum += vec4(textureGather4x1OffsetQCOM(isamp2d, c2D, offset));
    sum += vec4(textureGather4x1OffsetQCOM(isamp2d, c2D, offset, 1));
    sum += vec4(textureGather4x1OffsetQCOM(fsamp2d, c2D, offset));
    sum += vec4(textureGather4x1OffsetQCOM(fsamp2d, c2D, offset, 2));

    sum += vec4(textureGatherV2OffsetQCOM(usamp2d, c2D, offset));
    sum += vec4(textureGatherV2OffsetQCOM(usamp2d, c2D, offset, 1));
    sum += vec4(textureGatherV2OffsetQCOM(isamp2d, c2D, offset));
    sum += vec4(textureGatherV2OffsetQCOM(isamp2d, c2D, offset, 2));
    sum += vec4(textureGatherV2OffsetQCOM(fsamp2d, c2D, offset));
    sum += vec4(textureGatherV2OffsetQCOM(fsamp2d, c2D, offset, 3));

    sum += vec4(textureGatherH2OffsetQCOM(usamp2d, c2D, offset));
    sum += vec4(textureGatherH2OffsetQCOM(usamp2d, c2D, offset, 0));
    sum += vec4(textureGatherH2OffsetQCOM(isamp2d, c2D, offset));
    sum += vec4(textureGatherH2OffsetQCOM(isamp2d, c2D, offset, 1));
    sum += vec4(textureGatherH2OffsetQCOM(fsamp2d, c2D, offset));
    sum += vec4(textureGatherH2OffsetQCOM(fsamp2d, c2D, offset, 2));

    sum += vec4(textureGatherDOffsetQCOM(usamp2d, c2D, offset));
    sum += vec4(textureGatherDOffsetQCOM(usamp2d, c2D, offset, 1));
    sum += vec4(textureGatherDOffsetQCOM(isamp2d, c2D, offset));
    sum += vec4(textureGatherDOffsetQCOM(isamp2d, c2D, offset, 2));
    sum += vec4(textureGatherDOffsetQCOM(fsamp2d, c2D, offset));
    sum += vec4(textureGatherDOffsetQCOM(fsamp2d, c2D, offset, 3));

    sum += vec4(textureGather4x1OffsetQCOM(usamp2da, c3D, offset));
    sum += vec4(textureGather4x1OffsetQCOM(isamp2da, c3D, offset));
    sum += vec4(textureGather4x1OffsetQCOM(fsamp2da, c3D, offset));

    sum += vec4(textureGatherV2OffsetQCOM(usamp2da, c3D, offset));
    sum += vec4(textureGatherV2OffsetQCOM(isamp2da, c3D, offset));
    sum += vec4(textureGatherV2OffsetQCOM(fsamp2da, c3D, offset));

    sum += vec4(textureGatherH2OffsetQCOM(usamp2da, c3D, offset));
    sum += vec4(textureGatherH2OffsetQCOM(isamp2da, c3D, offset));
    sum += vec4(textureGatherH2OffsetQCOM(fsamp2da, c3D, offset));

    sum += vec4(textureGatherDOffsetQCOM(usamp2da, c3D, offset));
    sum += vec4(textureGatherDOffsetQCOM(isamp2da, c3D, offset));
    sum += vec4(textureGatherDOffsetQCOM(fsamp2da, c3D, offset));

    FragColor = sum;
}
