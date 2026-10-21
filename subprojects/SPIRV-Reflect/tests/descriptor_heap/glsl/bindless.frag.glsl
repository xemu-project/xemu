// Compile: glslang -V --target-env vulkan1.3 -S frag -e main -o bindless.frag.spv bindless.frag.glsl
/*
 * Copyright (c) 2024-2026, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2024-2026, NVIDIA CORPORATION.
 * SPDX-License-Identifier: Apache-2.0
 */
#version 450

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_descriptor_heap : enable

#include "shaderio.h"

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormal;
layout(location = 2) flat in int inFaceIdx;
layout(location = 3) flat in uint inBaseFaceTexIdx;

layout(location = 0) out vec4 outColor;

// Bindless mode: direct descriptor heap access via layout(descriptor_heap).
// No set/binding mapping needed. The shader computes texIdx = baseFaceTexIdx +
// faceIdx and indexes heapTextures[] directly. baseFaceTexIdx was computed in
// the vertex shader from gl_InstanceIndex.
layout(descriptor_heap) uniform texture2D heapTextures[];
layout(descriptor_heap) uniform sampler heapSamplers[];

layout(push_constant) uniform PushConstants_
{
  FrameInfo        frame;
  BindlessPushData bindless;
};

vec3 unpackColor(uint c)
{
  return vec3(float(c & 0xFFu), float((c >> 8) & 0xFFu), float((c >> 16) & 0xFFu)) / 255.0;
}

void main()
{
  uint texIdx   = inBaseFaceTexIdx + inFaceIdx;
  vec4 texColor = texture(sampler2D(heapTextures[nonuniformEXT(texIdx)], heapSamplers[0]), inUV);

  // Replace border pixels with per-draw-call border color
  float borderWidth = 1.0 / 48.0;
  if(inUV.x < borderWidth || inUV.x > 1.0 - borderWidth || inUV.y < borderWidth || inUV.y > 1.0 - borderWidth)
  {
    texColor.rgb = unpackColor(bindless.borderColor);
  }

  vec3  N     = normalize(inNormal);
  float NdotL = max(dot(N, normalize(frame.lightDir)), 0.0);
  outColor    = vec4(texColor.rgb * (0.3 + 0.7 * NdotL), 1.0);
}
