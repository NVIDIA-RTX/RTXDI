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

#ifndef RESERVOIR_VISUALIZATION_HLSLI
#define RESERVOIR_VISUALIZATION_HLSLI

#include "SharedShaderInclude/ShaderDebug/ReservoirSubfieldVizPasses/ReservoirVisualizationParameters.h"
#include "ReservoirVizUtils.hlsli"

static const float3 reservoirVizLuminanceWeights = float3(0.2126, 0.7152, 0.0722);
static const float3 reservoirVizNeutralBackgroundColor = float3(0.08, 0.08, 0.08);

struct ReservoirVisualizationResult
{
    float3 color;
    uint status;
};

ReservoirVisualizationResult ReservoirVizResult(float3 color, uint status)
{
    ReservoirVisualizationResult result;
    result.color = color;
    result.status = status;
    return result;
}

ReservoirVisualizationResult ReservoirVizValid(float3 color)
{
    return ReservoirVizResult(color, RESERVOIR_VIZ_STATUS_VALID);
}

float4 ReservoirVizDisplayColor(float3 color)
{
    float3 linearColor;
    [unroll]
    for (uint channel = 0; channel < 3; ++channel)
    {
        linearColor[channel] = color[channel] <= 0.04045
            ? color[channel] / 12.92
            : pow((color[channel] + 0.055) / 1.055, 2.4);
    }
    return float4(linearColor, 1.0);
}

bool ReservoirVizIsFinite(float value)
{
    // Inspect IEEE bits so classification survives ordinary shader fast math.
    return (asuint(value) & 0x7fffffff) < 0x7f800000;
}

bool ReservoirVizIsFinite(float3 value)
{
    return ReservoirVizIsFinite(value.x)
        && ReservoirVizIsFinite(value.y)
        && ReservoirVizIsFinite(value.z);
}

ReservoirVisualizationResult ReservoirVizEmpty(uint2 pixelPosition)
{
    return ReservoirVizResult(0.0, RESERVOIR_VIZ_STATUS_EMPTY);
}

ReservoirVisualizationResult ReservoirVizUnavailable(uint2 pixelPosition)
{
    return ReservoirVizResult(0.0, RESERVOIR_VIZ_STATUS_UNAVAILABLE);
}

ReservoirVisualizationResult ReservoirVizInactive(uint2 pixelPosition)
{
    return ReservoirVizResult(0.0, RESERVOIR_VIZ_STATUS_INACTIVE);
}

ReservoirVisualizationResult ReservoirVizInvalid(uint2 pixelPosition)
{
    return ReservoirVizResult(0.0, RESERVOIR_VIZ_STATUS_INVALID);
}

uint ReservoirVizRangeStatus(bool below, bool above)
{
    if (below && above)
    {
        return RESERVOIR_VIZ_STATUS_OUTSIDE_BOTH;
    }
    if (below)
    {
        return RESERVOIR_VIZ_STATUS_BELOW_RANGE;
    }
    return above ? RESERVOIR_VIZ_STATUS_ABOVE_RANGE : RESERVOIR_VIZ_STATUS_VALID;
}

float4 ReservoirVizStatusColor(uint status)
{
    return ReservoirVizDisplayColor(reservoirVizStatusColors[min(status, uint(RESERVOIR_VIZ_STATUS_COUNT - 1))]);
}

float4 ResolveReservoirVisualization(ReservoirVisualizationResult result, ReservoirVisualizationParameters settings)
{
    if (settings.displayMode == RESERVOIR_VIZ_DISPLAY_STATUS)
    {
        return ReservoirVizStatusColor(result.status);
    }
    if (result.status == RESERVOIR_VIZ_STATUS_INVALID || result.status >= RESERVOIR_VIZ_STATUS_EMPTY)
    {
        return ReservoirVizDisplayColor(reservoirVizNeutralBackgroundColor);
    }
    return float4(result.color, 1.0);
}

// Nine viridis samples, interpolated in display color space.
float4 ReservoirVizPalette(float value)
{
    const float3 viridisColors[9] = {
        float3(0.267, 0.005, 0.329), float3(0.279, 0.175, 0.483),
        float3(0.230, 0.322, 0.546), float3(0.173, 0.449, 0.558),
        float3(0.128, 0.567, 0.551), float3(0.158, 0.684, 0.502),
        float3(0.369, 0.789, 0.383), float3(0.678, 0.864, 0.190),
        float3(0.993, 0.906, 0.144)
    };
    float position = saturate(value) * 8.0;
    uint segment = min(uint(position), 7u);
    return ReservoirVizDisplayColor(lerp(viridisColors[segment], viridisColors[segment + 1], position - float(segment)));
}

float ReservoirVizScalarTransform(float value, uint scalarVizFunction)
{
    if (scalarVizFunction == RESERVOIR_VIZ_SCALAR_LOG2)
    {
        return log2(value);
    }
    if (scalarVizFunction == RESERVOIR_VIZ_SCALAR_LOG2_ONE_PLUS)
    {
        // Avoid cancellation in log2(1 + value) for small positive densities.
        if (value < 0.01)
        {
            return 1.44269504089 * value * (1.0 - value * (0.5 - value * (1.0 / 3.0 - value * 0.25)));
        }
        return log2(1.0 + value);
    }
    return value;
}

ReservoirVisualizationResult VisualizeReservoirScalar(float value, ReservoirVisualizationParameters settings, uint2 pixelPosition)
{
    if (!ReservoirVizIsFinite(value) || value < 0.0
        || !ReservoirVizIsFinite(settings.scalarMin) || !ReservoirVizIsFinite(settings.scalarMax)
        || settings.scalarMin < 0.0 || settings.scalarMax <= settings.scalarMin || settings.scalarVizFunction > RESERVOIR_VIZ_SCALAR_LOG2_ONE_PLUS
        || (settings.scalarVizFunction == RESERVOIR_VIZ_SCALAR_LOG2 && settings.scalarMin <= 0.0))
    {
        return ReservoirVizInvalid(pixelPosition);
    }
    uint status = ReservoirVizRangeStatus(value < settings.scalarMin, value > settings.scalarMax);

    if (value == 0.0 && status == RESERVOIR_VIZ_STATUS_VALID)
    {
        return ReservoirVizValid(0.0);
    }
    float lowerBound = ReservoirVizScalarTransform(settings.scalarMin, settings.scalarVizFunction);
    float upperBound = ReservoirVizScalarTransform(settings.scalarMax, settings.scalarVizFunction);
    if (upperBound <= lowerBound)
    {
        return ReservoirVizInvalid(pixelPosition);
    }
    float mappedValue = clamp(value, settings.scalarMin, settings.scalarMax);
    float normalizedValue = (ReservoirVizScalarTransform(mappedValue, settings.scalarVizFunction) - lowerBound)
        / (upperBound - lowerBound);
    return ReservoirVizResult(ReservoirVizPalette(normalizedValue).rgb, status);
}

float4 ReservoirVizSignedAxis(float value)
{
    const float3 center = float3(0.85, 0.85, 0.85);
    const float3 negative = float3(0.23, 0.30, 0.75);
    const float3 positive = float3(0.85, 0.33, 0.10);
    return ReservoirVizDisplayColor(lerp(center, value < 0.0 ? negative : positive, saturate(abs(value))));
}

float ReservoirVizLength(float3 value)
{
    if (!ReservoirVizIsFinite(value))
    {
        return any((asuint(value) & 0x7fffffff) > 0x7f800000) ? asfloat(0x7fc00000u) : asfloat(0x7f800000u);
    }
    float maximum = max(abs(value.x), max(abs(value.y), abs(value.z)));
    if (maximum == 0.0)
    {
        return 0.0;
    }
    return maximum * length(value / maximum);
}

ReservoirVisualizationResult VisualizeReservoirPosition(float3 value, ReservoirVisualizationParameters settings, uint2 pixelPosition)
{
    if (!ReservoirVizIsFinite(value) || !ReservoirVizIsFinite(settings.positionCenter)
        || !ReservoirVizIsFinite(settings.positionExtent) || settings.positionExtent <= 0.0 || settings.vectorVizMode > RESERVOIR_VIZ_POSITION_DISTANCE)
    {
        return ReservoirVizInvalid(pixelPosition);
    }
    float3 offset = value - settings.positionCenter;
    if (settings.vectorVizMode == RESERVOIR_VIZ_POSITION_DISTANCE)
    {
        float distance = ReservoirVizLength(offset);
        if (!ReservoirVizIsFinite(distance))
        {
            ReservoirVisualizationResult result = VisualizeReservoirScalar(settings.scalarMax, settings, pixelPosition);
            if (result.status != RESERVOIR_VIZ_STATUS_INVALID)
            {
                result.status = RESERVOIR_VIZ_STATUS_ABOVE_RANGE;
            }
            return result;
        }
        return VisualizeReservoirScalar(distance, settings, pixelPosition);
    }
    // Scale before subtraction to avoid float overflows
    if (!ReservoirVizIsFinite(offset))
    {
        offset = value / settings.positionExtent - settings.positionCenter / settings.positionExtent;
    }
    else
    {
        offset /= settings.positionExtent;
    }
    float3 visibleComponents = offset;
    if (settings.vectorVizMode != RESERVOIR_VIZ_POSITION_XYZ)
    {
        visibleComponents = offset[settings.vectorVizMode - RESERVOIR_VIZ_POSITION_X].xxx;
    }
    uint status = ReservoirVizRangeStatus(any(visibleComponents < -1.0), any(visibleComponents > 1.0));
    float3 color = ReservoirVizDisplayColor(saturate(offset * 0.5 + 0.5)).rgb;
    if (settings.vectorVizMode != RESERVOIR_VIZ_POSITION_XYZ)
    {
        color = ReservoirVizSignedAxis(clamp(visibleComponents.x, -1.0, 1.0)).rgb;
    }
    return ReservoirVizResult(color, status);
}

ReservoirVisualizationResult VisualizeReservoirNormal(float3 value, ReservoirVisualizationParameters settings, uint2 pixelPosition)
{
    if (!ReservoirVizIsFinite(value) || settings.vectorVizMode > RESERVOIR_VIZ_NORMAL_Z
        || dot(value, value) == 0.0 || any(abs(value) > 1.0001))
    {
        return ReservoirVizInvalid(pixelPosition);
    }
    float3 color = ReservoirVizDisplayColor(saturate(value * 0.5 + 0.5)).rgb;
    if (settings.vectorVizMode != RESERVOIR_VIZ_NORMAL_RGB)
    {
        color = ReservoirVizSignedAxis(value[settings.vectorVizMode - RESERVOIR_VIZ_NORMAL_X]).rgb;
    }
    return ReservoirVizValid(color);
}

ReservoirVisualizationResult VisualizeReservoirRadiance(float3 value, ReservoirVisualizationParameters settings, uint2 pixelPosition)
{
    if (!ReservoirVizIsFinite(value) || any(value < 0.0))
    {
        return ReservoirVizInvalid(pixelPosition);
    }
    float maximum = max(value.x, max(value.y, value.z));
    if (maximum == 0.0)
    {
        if (settings.colorMode == 1)
        {
            return VisualizeReservoirScalar(0.0, settings, pixelPosition);
        }
        return ReservoirVizValid(0.0);
    }
    float3 normalizedColor = value / maximum;
    float normalizedLuminance = dot(normalizedColor, reservoirVizLuminanceWeights);
    if (settings.colorMode == 1)
    {
        return VisualizeReservoirScalar(maximum * normalizedLuminance, settings, pixelPosition);
    }
    if (!ReservoirVizIsFinite(settings.exposure) || settings.colorMode != 0)
    {
        return ReservoirVizInvalid(pixelPosition);
    }
    // Luminance compression in log space avoids overflow when applying exposure.
    float logLuminance = log2(maximum) + log2(normalizedLuminance) + settings.exposure;
    float mappedLuminance;
    if (logLuminance >= 0.0)
    {
        mappedLuminance = 1.0 / (1.0 + exp2(-min(logLuminance, 126.0)));
    }
    else
    {
        float exposedLuminance = exp2(max(logLuminance, -126.0));
        mappedLuminance = exposedLuminance / (1.0 + exposedLuminance);
    }
    float3 color = normalizedColor * (mappedLuminance / normalizedLuminance);
    color /= max(1.0, max(color.x, max(color.y, color.z)));
    return ReservoirVizValid(color);
}

ReservoirVisualizationResult VisualizeReservoirDirection(float2 value, ReservoirVisualizationParameters settings, uint2 pixelPosition)
{
    const float3 zeroDisplacementColor = float3(0.5, 0.5, 0.5);
    const float3 rgbHueOffsets = float3(0.0, 2.0 / 3.0, 1.0 / 3.0);
    if (!ReservoirVizIsFinite(float3(value, 0.0)))
    {
        return ReservoirVizInvalid(pixelPosition);
    }
    float magnitude = ReservoirVizLength(float3(value, 0.0));
    if (settings.vectorVizMode == RESERVOIR_VIZ_DISPLACEMENT_MAGNITUDE)
    {
        return VisualizeReservoirScalar(magnitude, settings, pixelPosition);
    }
    if (!ReservoirVizIsFinite(settings.displacementSaturationDistance) || settings.displacementSaturationDistance <= 0.0 || settings.vectorVizMode != RESERVOIR_VIZ_DISPLACEMENT_DIRECTION_AND_MAGNITUDE)
    {
        return ReservoirVizInvalid(pixelPosition);
    }
    if (magnitude == 0.0)
    {
        return ReservoirVizValid(ReservoirVizDisplayColor(zeroDisplacementColor).rgb);
    }
    float hue = frac(atan2(value.y, value.x) / 6.28318530718 + 1.0);
    float3 directionColor = saturate(abs(frac(hue + rgbHueOffsets) * 6.0 - 3.0) - 1.0);
    float3 color = ReservoirVizDisplayColor(lerp(zeroDisplacementColor, directionColor, saturate(magnitude / settings.displacementSaturationDistance))).rgb;
    return ReservoirVizResult(color, ReservoirVizRangeStatus(false, magnitude > settings.displacementSaturationDistance));
}

ReservoirVisualizationResult VisualizeReservoirHash(uint value)
{
    return ReservoirVizValid(ReservoirVizDisplayColor(IndexToColor(value).rgb).rgb);
}

ReservoirVisualizationResult VisualizeReservoirHash(uint firstValue, uint secondValue)
{
    // Keep all input bits through hashing; don't convert to float before hashing.
    uint combinedHash = Murmur3(firstValue) ^ Murmur3(secondValue + 0x9e3779b9u);
    return VisualizeReservoirHash(combinedHash);
}

ReservoirVisualizationResult VisualizeReservoirReferenceMatch(uint value, uint referenceValue)
{
    const float3 matchingReferenceColor = float3(1.0, 1.0, 1.0);
    bool matches = value == referenceValue;
    return ReservoirVizValid(ReservoirVizDisplayColor(matches ? matchingReferenceColor : reservoirVizNeutralBackgroundColor).rgb);
}

#endif // RESERVOIR_VISUALIZATION_HLSLI
