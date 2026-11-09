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

#ifndef PT_RESERVOIR_SUBFIELD_VIZ_HLSL
#define PT_RESERVOIR_SUBFIELD_VIZ_HLSL

#include <donut/shaders/utils.hlsli>
#include <Rtxdi/PT/ReSTIRPTParameters.h>
#include "SharedShaderInclude/ShaderParameters.h"
#include "SharedShaderInclude/ShaderDebug/ReservoirSubfieldVizPasses/PTReservoirVizParameters.h"
#include "ReservoirVisualization.hlsli"

ConstantBuffer<PTReservoirVizParameters> g_Const : register(b0);
ConstantBuffer<ResamplingConstants> g_ConstRendering : register(b1);

RWStructuredBuffer<RTXDI_PackedPTReservoir> u_PTReservoirs : register(u0);
#define RTXDI_PT_RESERVOIR_BUFFER u_PTReservoirs

RWTexture2D<float4> t_Output : register(u1);
RWStructuredBuffer<uint> u_VisualizationCounts : register(u2);

#include "ReservoirVisualizationOutput.hlsli"

#include "Rtxdi/PT/Reservoir.hlsli"

static const float3 ptNeeLightPayloadColor = float3(1.0, 0.55, 0.10);
static const float3 ptRadiancePayloadColor = float3(0.15, 0.55, 1.0);

ReservoirVisualizationResult VisualizePTRandomIndex(RTXDI_PTReservoir reservoir, ReservoirVisualizationParameters settings, uint2 pixelPosition)
{
    // The index is an ordered counter. Hash only the complete replay state when requested.
    switch (settings.rngVizMode)
    {
    case RESERVOIR_VIZ_RNG_INDEX:
        return VisualizeReservoirScalar(float(reservoir.randomIndex), settings, pixelPosition);
    case RESERVOIR_VIZ_RNG_SEED_INDEX_HASH:
        return VisualizeReservoirHash(reservoir.randomSeed, reservoir.randomIndex);
    default:
        return ReservoirVizInvalid(pixelPosition);
    }
}

ReservoirVisualizationResult VisualizePTRadiance(RTXDI_PTReservoir reservoir, ReservoirVisualizationParameters settings, uint2 pixelPosition)
{
    if (settings.colorMode > RESERVOIR_VIZ_PT_RADIANCE_PAYLOAD_TYPE)
    {
        return ReservoirVizInvalid(pixelPosition);
    }

    // Inspect tag bits so fast math preserves the intentional infinity marker.
    const bool hasNeeLightTag = (asuint(reservoir.radiance.x) & 0x7fffffff) == 0x7f800000;
    const bool connectsToNeeLight = reservoir.rcVertexLength == reservoir.pathLength && hasNeeLightTag;
    if (connectsToNeeLight)
    {
        if (settings.colorMode == RESERVOIR_VIZ_PT_RADIANCE_RGB || settings.colorMode == RESERVOIR_VIZ_PT_RADIANCE_LUMINANCE)
        {
            return ReservoirVizUnavailable(pixelPosition);
        }

        const RTXDI_SampledLightData sampledLight = RTXDI_GetSampledLightData(reservoir);
        if (!RTXDI_SampledLightData_IsValidLightData(sampledLight))
        {
            return ReservoirVizInvalid(pixelPosition);
        }
        if (settings.colorMode == RESERVOIR_VIZ_PT_RADIANCE_PAYLOAD_TYPE)
        {
            return ReservoirVizValid(ReservoirVizDisplayColor(ptNeeLightPayloadColor).rgb);
        }
        if (settings.colorMode == RESERVOIR_VIZ_PT_RADIANCE_LIGHT_UV)
        {
            return ReservoirVizValid(ReservoirVizDisplayColor(float3(RTXDI_SampledLightData_GetUVDataFloat2(sampledLight), 0.0)).rgb);
        }
        return VisualizeReservoirHash(RTXDI_SampledLightData_GetLightIndex(sampledLight));
    }

    if (settings.colorMode == RESERVOIR_VIZ_PT_RADIANCE_LIGHT_ID || settings.colorMode == RESERVOIR_VIZ_PT_RADIANCE_LIGHT_UV)
    {
        return ReservoirVizUnavailable(pixelPosition);
    }
    if (settings.colorMode == RESERVOIR_VIZ_PT_RADIANCE_PAYLOAD_TYPE)
    {
        if (!ReservoirVizIsFinite(reservoir.radiance) || any(reservoir.radiance < 0.0))
        {
            return ReservoirVizInvalid(pixelPosition);
        }
        return ReservoirVizValid(ReservoirVizDisplayColor(ptRadiancePayloadColor).rgb);
    }
    return VisualizeReservoirRadiance(reservoir.radiance, settings, pixelPosition);
}

ReservoirVisualizationResult VisualizePTReservoir(RTXDI_PTReservoir reservoir, uint2 pixelPosition)
{
    const ReservoirVisualizationParameters settings = g_Const.visualization;

    if (reservoir.M == 0.0
        && g_Const.ptReservoirField != PT_RESERVOIR_FIELD_M
        && g_Const.ptReservoirField != PT_RESERVOIR_FIELD_WEIGHT_SUM)
    {
        return ReservoirVizEmpty(pixelPosition);
    }

    const bool hasReconnection = reservoir.rcVertexLength <= reservoir.pathLength;

    switch (g_Const.ptReservoirField)
    {
    case PT_RESERVOIR_FIELD_M:
        return VisualizeReservoirScalar(reservoir.M, settings, pixelPosition);
    case PT_RESERVOIR_FIELD_WEIGHT_SUM:
        return VisualizeReservoirScalar(reservoir.weightSum, settings, pixelPosition);
    case PT_RESERVOIR_FIELD_TRANSLATED_WORLD_POSITION:
        if (!hasReconnection)
        {
            return ReservoirVizUnavailable(pixelPosition);
        }
        return VisualizeReservoirPosition(reservoir.translatedWorldPosition, settings, pixelPosition);
    case PT_RESERVOIR_FIELD_WORLD_NORMAL:
        if (!hasReconnection)
        {
            return ReservoirVizUnavailable(pixelPosition);
        }
        return VisualizeReservoirNormal(reservoir.worldNormal, settings, pixelPosition);
    case PT_RESERVOIR_FIELD_RADIANCE:
        return VisualizePTRadiance(reservoir, settings, pixelPosition);
    case PT_RESERVOIR_FIELD_AGE:
        return VisualizeReservoirScalar(float(reservoir.age), settings, pixelPosition);
    case PT_RESERVOIR_FIELD_RC_WI_PDF:
        // Only an interior reconnection vertex has an outgoing BSDF sampling density.
        if (reservoir.rcVertexLength >= reservoir.pathLength)
        {
            return ReservoirVizUnavailable(pixelPosition);
        }
        return VisualizeReservoirScalar(reservoir.rcWiPdf, settings, pixelPosition);
    case PT_RESERVOIR_FIELD_PARTIAL_JACOBIAN:
        if (!hasReconnection)
        {
            return ReservoirVizUnavailable(pixelPosition);
        }
        return VisualizeReservoirScalar(reservoir.partialJacobian, settings, pixelPosition);
    case PT_RESERVOIR_FIELD_RC_VERTEX_LENGTH:
        return VisualizeReservoirScalar(float(reservoir.rcVertexLength), settings, pixelPosition);
    case PT_RESERVOIR_FIELD_PATH_LENGTH:
        return VisualizeReservoirScalar(float(reservoir.pathLength), settings, pixelPosition);
    case PT_RESERVOIR_FIELD_RANDOM_SEED:
        switch (settings.rngVizMode)
        {
        case RESERVOIR_VIZ_RNG_SEED_HASH:
            return VisualizeReservoirHash(reservoir.randomSeed);
        case RESERVOIR_VIZ_RNG_SEED_INDEX_HASH:
            return VisualizeReservoirHash(reservoir.randomSeed, reservoir.randomIndex);
        default:
            return ReservoirVizInvalid(pixelPosition);
        }
    case PT_RESERVOIR_FIELD_RANDOM_INDEX:
        return VisualizePTRandomIndex(reservoir, settings, pixelPosition);
    case PT_RESERVOIR_FIELD_TARGET_FUNCTION:
        return VisualizeReservoirRadiance(reservoir.targetFunction, settings, pixelPosition);
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
            const RTXDI_PTReservoir reservoir = RTXDI_LoadPTReservoir(g_Const.reservoirBufferParams,
                reservoirPosition, g_Const.bufferIndices.finalShadingInputBufferIndex);
            result = VisualizePTReservoir(reservoir, pixelPosition);
        }
    }
    WriteReservoirVisualization(result, pixelPosition, groupIndex, insideViewport, g_Const.visualization);
}

#endif // PT_RESERVOIR_SUBFIELD_VIZ_HLSL
