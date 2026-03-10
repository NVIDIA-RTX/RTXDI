#ifndef RAB_DENOISER_CALLBACK_HLSLI
#define RAB_DENOISER_CALLBACK_HLSLI

#include "../RAB_Surface.hlsli"

#include "Rtxdi/PT/Reservoir.hlsli"

void RAB_ReconnectionDenoiserCallback(const RTXDI_PTReservoir neighborSample,
                                      const RAB_Surface rcPrevSurface,
                                      inout RAB_PathTracerUserData ptud)
{
}

void RAB_LastBounceDenoiserCallback(const float3 lightPos,
                                    const RAB_Surface preLightSurface,
                                    inout RAB_PathTracerUserData ptud)
{
}

#endif // RAB_DENOISER_CALLBACK_HLSLI
