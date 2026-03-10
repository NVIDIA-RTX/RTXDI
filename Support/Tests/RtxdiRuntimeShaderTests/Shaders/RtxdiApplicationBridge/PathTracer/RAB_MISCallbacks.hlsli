#ifndef RAB_PATH_TRACER_MIS_CALLBACKS
#define RAB_PATH_TRACER_MIS_CALLBACKS

#include "Rtxdi/Utils/BrdfRaySample.hlsli"

float RAB_GetMISWeightForNEE(
    uint lightIndex,
    const RAB_LightSample lightSample,
    float3 lightDirection,
    float lightSolidAnglePdf,
    float scatterPdf)
{
    return 0;
}

float GetMISWeightForNEELight(
    uint lightIndex,
    RAB_LightSample lightSample,
    float3 shadingWorldPos,
    float scatterPdf)
{
    return 0;
}

float GetMISWeightForEmissiveSurface(
    RAB_RayPayload rayPayload,
    RAB_Surface prevSurface,
    RTXDI_BrdfRaySample brs)
{
    return 0;
}

float GetMISWeightForEnvironmentMap(float3 direction, RAB_Surface prevSurface, RTXDI_BrdfRaySample brs)
{
    return 0;
}

#endif // RAB_PATH_TRACER_MIS_CALLBACKS
