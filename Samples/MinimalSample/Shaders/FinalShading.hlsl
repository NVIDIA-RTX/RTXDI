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

#include <Rtxdi/DI/ReservoirStorage.hlsli>

[numthreads(RTXDI_SCREEN_SPACE_GROUP_SIZE, RTXDI_SCREEN_SPACE_GROUP_SIZE, 1)]
void main(uint2 pixelPosition : SV_DispatchThreadID)
{
    if (any(pixelPosition >= g_Const.view.viewportSize))
    {
        return;
    }

    RAB_Surface surface = RAB_GetGBufferSurface(pixelPosition, false);
    RTXDI_DIReservoir reservoir = RTXDI_EmptyDIReservoir();
    float3 shadingOutput = 0;

    if (RAB_IsSurfaceValid(surface))
    {
        reservoir = RTXDI_LoadDIReservoir(g_Const.restirDIReservoirBufferParams, pixelPosition, g_Const.bufferIndices.shadingInputBufferIndex);

        if (RTXDI_IsValidDIReservoir(reservoir))
        {
            RAB_LightSample lightSample = RAB_SamplePolymorphicLight(
                RAB_LoadLightInfo(RTXDI_GetDIReservoirLightIndex(reservoir), false),
                surface, RTXDI_GetDIReservoirSampleUV(reservoir));

            bool visible = RAB_GetConservativeVisibility(surface, lightSample);
            if (visible)
            {
                shadingOutput = ShadeSurfaceWithLightSample(lightSample, surface) * RTXDI_GetDIReservoirInvPdf(reservoir);
            }
            RTXDI_StoreVisibilityInDIReservoir(reservoir, visible ? 1.0 : 0.0, true);
        }

        shadingOutput += u_ShadingOutput[pixelPosition].rgb;
        shadingOutput = BasicToneMapping(shadingOutput, 0.005);
    }

    u_ShadingOutput[pixelPosition] = float4(shadingOutput, 0);
    // Save final visibility with the reservoir that temporal reuse reads next frame.
    RTXDI_StoreDIReservoir(reservoir, g_Const.restirDIReservoirBufferParams, pixelPosition, g_Const.bufferIndices.shadingInputBufferIndex);
}
