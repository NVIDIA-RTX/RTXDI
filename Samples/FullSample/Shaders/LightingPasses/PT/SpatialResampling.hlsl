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

#include "../RtxdiApplicationBridge/RtxdiApplicationBridge.hlsli"
#define RTXDI_RESTIR_PT_HYBRID_SHIFT
#include "../RtxdiApplicationBridge/PathTracer/RAB_PathTracer.hlsli"
#include "../../ShaderDebug/ShaderDebugPrint/ShaderDebugPrint.hlsli"

#include <Rtxdi/PT/SpatialResampling.hlsli>

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
    const uint2 PixelPosition = RTXDI_ReservoirPosToPixelPos(GlobalIndex, g_Const.runtimeParams.activeCheckerboardField);
    const uint2 ReservoirPosition = RTXDI_PixelPosToReservoirPos(PixelPosition, g_Const.runtimeParams.activeCheckerboardField);

    ShaderDebug::SetDebugShaderPrintCurrentThreadCursorXY(PixelPosition);
    if (all(PixelPosition == g_Const.debug.mouseSelectedPixel))
    {
        Debug_EnablePTPathRecording();
    }

    RTXDI_PTSpatialResamplingRuntimeParameters srrParams = RTXDI_EmptyPTSpatialResamplingRuntimeParameters();
    srrParams.PixelPosition = PixelPosition;
    srrParams.ReservoirPosition = ReservoirPosition;
    srrParams.cameraPos = g_Const.view.cameraDirectionOrPosition.xyz;
    srrParams.prevCameraPos = g_Const.prevView.cameraDirectionOrPosition.xyz;
    srrParams.prevPrevCameraPos = g_Const.prevPrevView.cameraDirectionOrPosition.xyz;

    RTXDI_RandomSamplerState rng = RTXDI_InitRandomSampler(PixelPosition, g_Const.runtimeParams.frameIndex, RTXDI_PT_SPATIAL_RESAMPLING_RANDOM_SEED);
    RAB_PathTracerUserData ptud = RAB_EmptyPathTracerUserData();
    ptud.psr.primarySurfaceWorldPos = float3(0, 0, 0);
    ptud.psr.primarySurfaceViewDir = float3(0, 0, 0);
    ptud.psr.updateWithResampling = g_Const.updatePSRwithResampling;
    bool selectedNeighborSample = false;

    RTXDI_PTReservoir targetReservoir = RTXDI_PTSpatialResampling(srrParams, g_Const.restirPT.spatialResampling, g_Const.restirPT.hybridShift, g_Const.restirPT.reconnection, g_Const.restirPT.reservoirBuffer, g_Const.restirPT.bufferIndices, g_Const.runtimeParams, rng, selectedNeighborSample, ptud);

    if (selectedNeighborSample && ptud.psr.updateWithResampling && ptud.psr.hasRecorded())
    {
        UpdatePSRBuffers(ptud.psr, srrParams.PixelPosition, RTXDI_PTPathTraceInvocationType_Spatial);
    }

    RTXDI_StorePTReservoir(targetReservoir, g_Const.restirPT.reservoirBuffer, ReservoirPosition, g_Const.restirPT.bufferIndices.spatialResamplingOutputBufferIndex);
}
