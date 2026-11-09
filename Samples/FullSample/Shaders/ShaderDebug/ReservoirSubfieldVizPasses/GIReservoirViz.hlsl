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

#ifndef GI_RESERVOIR_SUBFIELD_VIZ_HLSL
#define GI_RESERVOIR_SUBFIELD_VIZ_HLSL

#include <donut/shaders/utils.hlsli>
#include <Rtxdi/GI/ReSTIRGIParameters.h>
#include "SharedShaderInclude/ShaderDebug/ReservoirSubfieldVizPasses/GIReservoirVizParameters.h"
#include "ReservoirVisualization.hlsli"

ConstantBuffer<GIReservoirVizParameters> g_Const : register(b0);

RWStructuredBuffer<RTXDI_PackedGIReservoir> u_GIReservoirs : register(u0);
#define RTXDI_GI_RESERVOIR_BUFFER u_GIReservoirs

RWTexture2D<float4> t_Output : register(u1);
RWStructuredBuffer<uint> u_VisualizationCounts : register(u2);

#include "ReservoirVisualizationOutput.hlsli"

#include "Rtxdi/GI/Reservoir.hlsli"

ReservoirVisualizationResult VisualizeGIReservoir(RTXDI_GIReservoir reservoir, uint2 pixelPosition)
{
    if (g_Const.giReservoirField == GI_RESERVOIR_FIELD_M)
    {
        return VisualizeReservoirScalar(float(reservoir.M), g_Const.visualization, pixelPosition);
    }
    if (g_Const.giReservoirField == GI_RESERVOIR_FIELD_WEIGHT_SUM)
    {
        return VisualizeReservoirScalar(reservoir.weightSum, g_Const.visualization, pixelPosition);
    }
    if (!RTXDI_IsValidGIReservoir(reservoir))
    {
        return ReservoirVizEmpty(pixelPosition);
    }

    switch (g_Const.giReservoirField)
    {
    case GI_RESERVOIR_FIELD_POSITION:
        return VisualizeReservoirPosition(reservoir.position, g_Const.visualization, pixelPosition);
    case GI_RESERVOIR_FIELD_NORMAL:
        return VisualizeReservoirNormal(reservoir.normal, g_Const.visualization, pixelPosition);
    case GI_RESERVOIR_FIELD_RADIANCE:
        return VisualizeReservoirRadiance(reservoir.radiance, g_Const.visualization, pixelPosition);
    case GI_RESERVOIR_FIELD_AGE:
        return VisualizeReservoirScalar(float(reservoir.age), g_Const.visualization, pixelPosition);
    default:
        return ReservoirVizUnavailable(pixelPosition);
    }
}

[numthreads(16, 16, 1)]
void main(uint2 pixelPosition : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex)
{
    const bool insideViewport = all(pixelPosition < uint2(g_Const.view.viewportSize));
    ReservoirVisualizationResult result = ReservoirVizInactive(pixelPosition);
    if (insideViewport)
    {
        const uint2 reservoirPosition = RTXDI_PixelPosToReservoirPos(pixelPosition, g_Const.runtimeParams.activeCheckerboardField);
        if (all(RTXDI_ReservoirPosToPixelPos(reservoirPosition, g_Const.runtimeParams.activeCheckerboardField) == pixelPosition))
        {
            const RTXDI_GIReservoir reservoir = RTXDI_LoadGIReservoir(g_Const.reservoirBufferParams,
                reservoirPosition, g_Const.bufferIndices.finalShadingInputBufferIndex);
            result = VisualizeGIReservoir(reservoir, pixelPosition);
        }
    }
    WriteReservoirVisualization(result, pixelPosition, groupIndex, insideViewport, g_Const.visualization);
}

#endif // GI_RESERVOIR_SUBFIELD_VIZ_HLSL
