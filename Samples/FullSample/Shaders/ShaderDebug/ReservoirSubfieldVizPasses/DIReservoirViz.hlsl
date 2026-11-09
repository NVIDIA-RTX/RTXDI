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

#ifndef DI_RESERVOIR_SUBFIELD_VIZ_HLSL
#define DI_RESERVOIR_SUBFIELD_VIZ_HLSL

#include <donut/shaders/utils.hlsli>
#include <Rtxdi/DI/ReSTIRDIParameters.h>
#include "SharedShaderInclude/ShaderParameters.h"
#include "SharedShaderInclude/ShaderDebug/ReservoirSubfieldVizPasses/DIReservoirVizParameters.h"
#include "ReservoirVisualization.hlsli"

ConstantBuffer<DIReservoirVizParameters> g_Const : register(b0);
ConstantBuffer<ResamplingConstants> g_ConstRendering : register(b1);

RWStructuredBuffer<RTXDI_PackedDIReservoir> u_DIReservoirs : register(u0);
#define RTXDI_LIGHT_RESERVOIR_BUFFER u_DIReservoirs

#include <Rtxdi/DI/ReservoirStorage.hlsli>

RWTexture2D<float4> t_Output : register(u1);
RWStructuredBuffer<uint> u_VisualizationCounts : register(u2);

#include "ReservoirVisualizationOutput.hlsli"

static const float3 diVisibilityReuseDisabledColor = float3(0.15, 0.15, 0.15);
static const float3 diVisibilityAgeZeroColor = float3(0.03, 0.25, 1.0);
static const float3 diVisibilityTooOldColor = float3(1.0, 0.3, 0.02);
static const float3 diVisibilityTooDistantColor = float3(0.45, 0.05, 0.8);
static const float3 diVisibilityReusableColor = float3(0.05, 0.8, 0.2);

ReservoirVisualizationResult VisualizeDIVisibility(RTXDI_DIReservoir reservoir)
{
    if (g_Const.visualization.colorMode == 2)
    {
        RTXDI_ShadingParameters shadingParams = g_ConstRendering.restirDI.shadingParams;
        if (!shadingParams.enableFinalVisibility || !shadingParams.reuseFinalVisibility)
        {
            return ReservoirVizValid(diVisibilityReuseDisabledColor);
        }
        if (reservoir.age == 0)
        {
            return ReservoirVizValid(diVisibilityAgeZeroColor);
        }
        if (reservoir.age > shadingParams.finalVisibilityMaxAge)
        {
            return ReservoirVizValid(diVisibilityTooOldColor);
        }
        if (length(float2(reservoir.spatialDistance)) >= shadingParams.finalVisibilityMaxDistance)
        {
            return ReservoirVizValid(diVisibilityTooDistantColor);
        }
        return ReservoirVizValid(diVisibilityReusableColor);
    }

    // Decode the stored channels directly. The reuse helper returns black when the
    // history is ineligible, which would incorrectly make it look fully occluded.
    float3 visibility = float3(
        reservoir.packedVisibility & RTXDI_PackedDIReservoir_VisibilityChannelMax,
        (reservoir.packedVisibility >> RTXDI_PackedDIReservoir_VisibilityChannelShift) & RTXDI_PackedDIReservoir_VisibilityChannelMax,
        (reservoir.packedVisibility >> (RTXDI_PackedDIReservoir_VisibilityChannelShift * 2)) & RTXDI_PackedDIReservoir_VisibilityChannelMax)
        / float(RTXDI_PackedDIReservoir_VisibilityChannelMax);

    if (g_Const.visualization.colorMode == 1)
    {
        visibility = dot(visibility, reservoirVizLuminanceWeights);
    }
    return ReservoirVizValid(visibility);
}

ReservoirVisualizationResult VisualizeDIReservoirUV(float2 uv)
{
    switch (g_Const.visualization.vectorVizMode)
    {
    case RESERVOIR_VIZ_UV_U_GRAYSCALE:
        return ReservoirVizValid(ReservoirVizDisplayColor(uv.xxx).rgb);
    case RESERVOIR_VIZ_UV_V_GRAYSCALE:
        return ReservoirVizValid(ReservoirVizDisplayColor(uv.yyy).rgb);
    case RESERVOIR_VIZ_UV_RG_COORDINATES:
    default:
        return ReservoirVizValid(ReservoirVizDisplayColor(float3(uv, 0.0)).rgb);
    }
}

ReservoirVisualizationResult VisualizeDIReservoir(RTXDI_PackedDIReservoir packedReservoir, uint2 pixelPosition)
{
    // Decode selected fields without the SDK loader's global nonfinite-weight
    // rejection: an unrelated bad weight must not hide UV, age, or the light index.
    RTXDI_PackedDIReservoir dataForDecode = packedReservoir;
    dataForDecode.weight = 1.0;
    RTXDI_DIReservoir reservoir = RTXDI_UnpackDIReservoir(dataForDecode);
    const bool hasSelectedSample = (reservoir.lightData & RTXDI_DIReservoir_LightValidBit) != 0;

    switch (g_Const.diReservoirField)
    {
    case DI_RESERVOIR_FIELD_LIGHT_DATA:
    {
        if (!hasSelectedSample)
        {
            return ReservoirVizEmpty(pixelPosition);
        }
        uint lightIndex = RTXDI_GetDIReservoirLightIndex(reservoir);
        if (lightIndex >= g_Const.maxLightsInBuffer)
        {
            lightIndex -= g_Const.maxLightsInBuffer;
        }
        if (lightIndex >= g_Const.maxLightsInBuffer)
        {
            return ReservoirVizInvalid(pixelPosition);
        }
        switch (g_Const.visualization.lightIdVizMode)
        {
        case RESERVOIR_VIZ_LIGHT_ID_HASH:
            return VisualizeReservoirHash(lightIndex);
        case RESERVOIR_VIZ_LIGHT_ID_MATCH_REFERENCE:
            return VisualizeReservoirReferenceMatch(lightIndex, g_Const.visualization.referenceLightIndex);
        default:
            return ReservoirVizInvalid(pixelPosition);
        }
    }
    case DI_RESERVOIR_FIELD_UV_DATA:
        if (!hasSelectedSample)
        {
            return ReservoirVizEmpty(pixelPosition);
        }
        return VisualizeDIReservoirUV(RTXDI_GetDIReservoirSampleUV(reservoir));
    case DI_RESERVOIR_FIELD_TARGET_PDF:
        if (!hasSelectedSample)
        {
            return ReservoirVizEmpty(pixelPosition);
        }
        return VisualizeReservoirScalar(reservoir.targetPdf, g_Const.visualization, pixelPosition);
    case DI_RESERVOIR_FIELD_M:
        return VisualizeReservoirScalar(reservoir.M, g_Const.visualization, pixelPosition);
    case DI_RESERVOIR_FIELD_PACKED_VISIBILITY:
        return VisualizeDIVisibility(reservoir);
    case DI_RESERVOIR_FIELD_SPATIAL_DISTANCE:
        return VisualizeReservoirDirection(float2(reservoir.spatialDistance), g_Const.visualization, pixelPosition);
    case DI_RESERVOIR_FIELD_AGE:
        return VisualizeReservoirScalar(float(reservoir.age), g_Const.visualization, pixelPosition);
    case DI_RESERVOIR_FIELD_CANONICAL_WEIGHT:
        // This temporary pairwise MIS value is not serialized into the reservoir.
        return ReservoirVizUnavailable(pixelPosition);
    default:
        if (!hasSelectedSample)
        {
            return ReservoirVizEmpty(pixelPosition);
        }
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
            const uint reservoirPointer = RTXDI_ReservoirPositionToPointer(g_Const.reservoirBufferParams,
                reservoirPosition, g_Const.bufferIndices.shadingInputBufferIndex);
            const RTXDI_PackedDIReservoir packedReservoir = u_DIReservoirs[reservoirPointer];
            result = VisualizeDIReservoir(packedReservoir, pixelPosition);
        }
    }
    WriteReservoirVisualization(result, pixelPosition, groupIndex, insideViewport, g_Const.visualization);
}

#endif // DI_RESERVOIR_SUBFIELD_VIZ_HLSL
