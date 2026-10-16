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

#ifndef SHADERIO_H
#define SHADERIO_H

// clang-format off

/*
 * Push data layout (used by vkCmdPushDataEXT):
 *   Offset 0:   FrameInfo (160 bytes) — pushed once per frame
 *   Offset 160: DrawData (80 bytes)   — per-draw mode: pushed per cube
 *           or: BindlessPushData      — bindless mode: pushed once
 *
 * In per-draw mode, DrawData::baseFaceTexIdx is NOT read by the shader.
 * Instead, the descriptor heap mapping (HEAP_WITH_PUSH_INDEX) reads it at
 * pushOffset to resolve the 6 face textures from the heap. The shader just
 * uses faceTextures[faceIdx] and the mapping translates that to heap access.
 */

// Pushed once per frame at offset 0
struct FrameInfo {
  float4x4 proj;
  float4x4 view;
  float3   lightDir;
  float    time;
  uint     numCubes;    // total cube count (for animation stagger)
  float    dropHeight;  // height for drop animation
  float    _pad0;
  float    _pad1;
};

// Per-draw mode: pushed per cube at offset 160
struct DrawData {
  float4x4 transform;
  uint     baseFaceTexIdx;  // heap index of first of 6 face textures (consumed by
                            // mapping, not shader)
  uint cubeIndex;           // sequential index for animation delay
  uint borderColor;         // packed RGBA8 border color (hashed from cubeIndex)
  uint _pad;
};

// Bindless mode: pushed once at offset 160
struct BindlessPushData {
  uint borderColor;  // packed RGBA8 border color for the single draw call
  uint gridSize;     // N for NxNxN grid; shader derives position from
                     // gl_InstanceIndex
};

// clang-format on

#endif
