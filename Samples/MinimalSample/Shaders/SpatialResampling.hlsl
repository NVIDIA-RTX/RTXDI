/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#pragma pack_matrix(row_major)

#define RTXDI_ENABLE_PRESAMPLING 0
#include "RtxdiApplicationBridge/RtxdiApplicationBridge.hlsli"

#include <Rtxdi/DI/SpatialResampling.hlsli>

[numthreads(RTXDI_SCREEN_SPACE_GROUP_SIZE, RTXDI_SCREEN_SPACE_GROUP_SIZE, 1)]
void main(uint2 pixelPosition : SV_DispatchThreadID)
{
    if (any(pixelPosition >= g_Const.view.viewportSize))
    {
        return;
    }

    RAB_Surface surface = RAB_GetGBufferSurface(pixelPosition, false);
    RTXDI_DIReservoir reservoir = RTXDI_EmptyDIReservoir();

    if (RAB_IsSurfaceValid(surface))
    {
        reservoir = RTXDI_LoadDIReservoir(g_Const.restirDIReservoirBufferParams, pixelPosition,
            g_Const.bufferIndices.spatialResamplingInputBufferIndex);

        RTXDI_RandomSamplerState rng = RTXDI_InitRandomSampler(pixelPosition, g_Const.runtimeParams.frameIndex, 2);
        RAB_LightSample lightSample = RAB_EmptyLightSample();
        reservoir = RTXDI_DISpatialResampling(pixelPosition, surface, reservoir, rng,
            g_Const.runtimeParams, g_Const.restirDIReservoirBufferParams,
            g_Const.bufferIndices.spatialResamplingInputBufferIndex,
            g_Const.spatialResamplingParams, lightSample);
    }

    RTXDI_StoreDIReservoir(reservoir, g_Const.restirDIReservoirBufferParams, pixelPosition,
        g_Const.bufferIndices.spatialResamplingOutputBufferIndex);
}
