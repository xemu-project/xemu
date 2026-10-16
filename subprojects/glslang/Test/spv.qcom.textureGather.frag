#version 460
#extension GL_QCOM_image_processing3 : enable

precision mediump float;
precision highp usampler2D;
precision highp sampler2D;
precision highp isampler2DArray;

layout(location = 0) in vec2 c2D;
layout(location = 1) in vec3 c3D;
layout(location = 2) in flat ivec2 offset;

layout(location = 3) out vec4 FragColor;

layout(binding = 0) uniform usampler2D usamp2d;
layout(binding = 1) uniform isampler2D isamp2d;
layout(binding = 2) uniform  sampler2D fsamp2d;

layout(binding = 3) uniform usampler2DArray usamp2da;
layout(binding = 4) uniform isampler2DArray isamp2da;
layout(binding = 5) uniform  sampler2DArray fsamp2da;

void main()
{
    vec4 sum = vec4(0.0f);
    //ivec2 offset = ivec2(0,0);

    sum += textureGather4x1QCOM(usamp2d, c2D);
    sum += textureGather4x1QCOM(usamp2d, c2D, 0);
    sum += textureGather4x1QCOM(isamp2d, c2D);
    sum += textureGather4x1QCOM(isamp2d, c2D, 1);
    sum += textureGather4x1QCOM(fsamp2d, c2D);
    sum += textureGather4x1QCOM(fsamp2d, c2D, 2);

    sum += textureGatherV2QCOM(usamp2d, c2D);
    sum += textureGatherV2QCOM(usamp2d, c2D, 1);
    sum += textureGatherV2QCOM(isamp2d, c2D);
    sum += textureGatherV2QCOM(isamp2d, c2D, 2);
    sum += textureGatherV2QCOM(fsamp2d, c2D);
    sum += textureGatherV2QCOM(fsamp2d, c2D, 3);

    sum += textureGatherH2QCOM(usamp2d, c2D);
    sum += textureGatherH2QCOM(usamp2d, c2D, 0);
    sum += textureGatherH2QCOM(isamp2d, c2D);
    sum += textureGatherH2QCOM(isamp2d, c2D, 1);
    sum += textureGatherH2QCOM(fsamp2d, c2D);
    sum += textureGatherH2QCOM(fsamp2d, c2D, 2);

    sum += textureGatherDQCOM(usamp2d, c2D);
    sum += textureGatherDQCOM(usamp2d, c2D, 1);
    sum += textureGatherDQCOM(isamp2d, c2D);
    sum += textureGatherDQCOM(isamp2d, c2D, 2);
    sum += textureGatherDQCOM(fsamp2d, c2D);
    sum += textureGatherDQCOM(fsamp2d, c2D, 3);

    sum += textureGather4x1QCOM(usamp2da, c3D);
    sum += textureGather4x1QCOM(isamp2da, c3D);
    sum += textureGather4x1QCOM(fsamp2da, c3D);

    sum += textureGatherV2QCOM(usamp2da, c3D);
    sum += textureGatherV2QCOM(isamp2da, c3D);
    sum += textureGatherV2QCOM(fsamp2da, c3D);

    sum += textureGatherH2QCOM(usamp2da, c3D);
    sum += textureGatherH2QCOM(isamp2da, c3D);
    sum += textureGatherH2QCOM(fsamp2da, c3D);

    sum += textureGatherDQCOM(usamp2da, c3D);
    sum += textureGatherDQCOM(isamp2da, c3D);
    sum += textureGatherDQCOM(fsamp2da, c3D);

    // textureGather*OffsetQCOM
    sum += textureGather4x1OffsetQCOM(usamp2d, c2D, offset);
    sum += textureGather4x1OffsetQCOM(usamp2d, c2D, offset, 0);
    sum += textureGather4x1OffsetQCOM(isamp2d, c2D, offset);
    sum += textureGather4x1OffsetQCOM(isamp2d, c2D, offset, 1);
    sum += textureGather4x1OffsetQCOM(fsamp2d, c2D, offset);
    sum += textureGather4x1OffsetQCOM(fsamp2d, c2D, offset, 2);

    sum += textureGatherV2OffsetQCOM(usamp2d, c2D, offset);
    sum += textureGatherV2OffsetQCOM(usamp2d, c2D, offset, 1);
    sum += textureGatherV2OffsetQCOM(isamp2d, c2D, offset);
    sum += textureGatherV2OffsetQCOM(isamp2d, c2D, offset, 2);
    sum += textureGatherV2OffsetQCOM(fsamp2d, c2D, offset);
    sum += textureGatherV2OffsetQCOM(fsamp2d, c2D, offset, 3);

    sum += textureGatherH2OffsetQCOM(usamp2d, c2D, offset);
    sum += textureGatherH2OffsetQCOM(usamp2d, c2D, offset, 0);
    sum += textureGatherH2OffsetQCOM(isamp2d, c2D, offset);
    sum += textureGatherH2OffsetQCOM(isamp2d, c2D, offset, 1);
    sum += textureGatherH2OffsetQCOM(fsamp2d, c2D, offset);
    sum += textureGatherH2OffsetQCOM(fsamp2d, c2D, offset, 2);

    sum += textureGatherDOffsetQCOM(usamp2d, c2D, offset);
    sum += textureGatherDOffsetQCOM(usamp2d, c2D, offset, 1);
    sum += textureGatherDOffsetQCOM(isamp2d, c2D, offset);
    sum += textureGatherDOffsetQCOM(isamp2d, c2D, offset, 2);
    sum += textureGatherDOffsetQCOM(fsamp2d, c2D, offset);
    sum += textureGatherDOffsetQCOM(fsamp2d, c2D, offset, 3);

    sum += textureGather4x1OffsetQCOM(usamp2da, c3D, offset);
    sum += textureGather4x1OffsetQCOM(isamp2da, c3D, offset);
    sum += textureGather4x1OffsetQCOM(fsamp2da, c3D, offset);

    sum += textureGatherV2OffsetQCOM(usamp2da, c3D, offset);
    sum += textureGatherV2OffsetQCOM(isamp2da, c3D, offset);
    sum += textureGatherV2OffsetQCOM(fsamp2da, c3D, offset);

    sum += textureGatherH2OffsetQCOM(usamp2da, c3D, offset);
    sum += textureGatherH2OffsetQCOM(isamp2da, c3D, offset);
    sum += textureGatherH2OffsetQCOM(fsamp2da, c3D, offset);

    sum += textureGatherDOffsetQCOM(usamp2da, c3D, offset);
    sum += textureGatherDOffsetQCOM(isamp2da, c3D, offset);
    sum += textureGatherDOffsetQCOM(fsamp2da, c3D, offset);

    FragColor = sum;
}
