#!/bin/bash
# Creates multi_entrypoint.spv from multi_entrypoint.glsl and
# multi_entrypoint.spv.dis.diff
glslc -fshader-stage=vert multi_entrypoint.glsl -o multi_entrypoint.spv
spirv-dis multi_entrypoint.spv > multi_entrypoint.spv.dis
patch multi_entrypoint.spv.dis multi_entrypoint.spv.dis.diff
spirv-as multi_entrypoint.spv.dis -o multi_entrypoint.spv
