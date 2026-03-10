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

/*
 * Fill sample ID texture from current frame's PT reservoirs (temporal output).
 * Used for dupmap-based correlation reduction: each pixel gets its reservoir's
 * RandomSeed (or 0 if invalid) so ComputeDuplicationMap can count duplicates.
 */

#pragma pack_matrix(row_major)

#include "../RtxdiApplicationBridge/RtxdiApplicationBridge.hlsli"

#include "Rtxdi/Utils/ReservoirAddressing.hlsli"
#include "Rtxdi/PT/Reservoir.hlsli"

#if USE_RAY_QUERY
[numthreads(RTXDI_SCREEN_SPACE_GROUP_SIZE, RTXDI_SCREEN_SPACE_GROUP_SIZE, 1)]
void main(uint2 GlobalIndex : SV_DispatchThreadID)
#else
[shader("raygeneration")]
void RayGen()
#endif
{
#if !USE_RAY_QUERY
    const uint2 GlobalIndex = DispatchRaysIndex().xy;
#endif
    const uint2 pixelPosition = RTXDI_ReservoirPosToPixelPos(GlobalIndex, g_Const.runtimeParams.activeCheckerboardField);
    const uint2 frameDim = uint2(g_Const.view.viewportSize);
    if (any(pixelPosition >= frameDim))
        return;

    const uint2 reservoirPosition = RTXDI_PixelPosToReservoirPos(pixelPosition, g_Const.runtimeParams.activeCheckerboardField);
    const uint outputBufferIndex = g_Const.restirPT.bufferIndices.finalShadingInputBufferIndex;
    RTXDI_PTReservoir reservoir = RTXDI_LoadPTReservoir(g_Const.restirPT.reservoirBuffer, reservoirPosition, outputBufferIndex);

    uint sampleID = (reservoir.WeightSum == 0.0) ? 0u : reservoir.RandomSeed;
    u_PTSampleIDTexture[pixelPosition] = sampleID;
}
