/*
 * SPDX-FileCopyrightText: Copyright (c) 2020-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#ifndef PACKED_NORMAL_HLSL
#define PACKED_NORMAL_HLSL

#include "SharedShaderInclude/ShaderParameters.h"

#include <donut/shaders/packing.hlsli>

Texture2D<uint> t_PackedVecs : register(t0);
RWTexture2D<float4> t_Output : register(u0);

[numthreads(16, 16, 1)]
void main(uint2 pixelPosition : SV_DispatchThreadID)
{
    uint packedVec = t_PackedVecs[pixelPosition];
    float3 unpackedVec = Unpack_R11G11B10_UFLOAT(packedVec);
    t_Output[pixelPosition] = float4(unpackedVec, 1.0);
}

#endif // PACKED_NORMAL_HLSL