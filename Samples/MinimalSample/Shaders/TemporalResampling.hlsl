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
#define RTXDI_ALLOWED_BIAS_CORRECTION RTXDI_BIAS_CORRECTION_RAY_TRACED
#include "RtxdiApplicationBridge/RtxdiApplicationBridge.hlsli"

#include <Rtxdi/DI/TemporalResampling.hlsli>

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
        reservoir = RTXDI_LoadDIReservoir(g_Const.restirDIReservoirBufferParams, pixelPosition, g_Const.bufferIndices.initialSamplingOutputBufferIndex);

        RTXDI_RandomSamplerState rng = RTXDI_InitRandomSampler(pixelPosition, g_Const.runtimeParams.frameIndex, 3);
        RAB_LightSample lightSample = RAB_EmptyLightSample();
        int2 temporalSamplePixelPos = int2(0, 0);
        reservoir = RTXDI_DITemporalResampling(pixelPosition, surface, reservoir,
            rng, g_Const.runtimeParams, g_Const.restirDIReservoirBufferParams, u_MotionVectors[pixelPosition].xyz,
            g_Const.bufferIndices.temporalResamplingInputBufferIndex, g_Const.temporalResamplingParams,
            temporalSamplePixelPos, lightSample);
    }

    RTXDI_StoreDIReservoir(reservoir, g_Const.restirDIReservoirBufferParams, pixelPosition,
        g_Const.bufferIndices.temporalResamplingOutputBufferIndex);
}
