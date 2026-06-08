#version 450
#extension GL_QCOM_image_processing : require

layout(set = 0, binding = 0) uniform texture2D inTex;
layout(set = 0, binding = 3) uniform sampler linearSampler;
layout(set = 0, binding = 1) uniform texture2DArray kernelTex;
layout(set = 0, binding = 4) uniform sampler weightSampler;

layout(location = 0) out vec4 fragColor;
layout(location = 0) in vec2 uv;

void main()
{
    vec4 _32 = textureWeightedQCOM(sampler2D(inTex, linearSampler), uv, sampler2DArray(kernelTex, weightSampler));
    fragColor = _32;
}

