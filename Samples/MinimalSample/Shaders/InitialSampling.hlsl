/***************************************************************************
 # Copyright (c) 2021-2023, NVIDIA CORPORATION.  All rights reserved.
 #
 # NVIDIA CORPORATION and its licensors retain all intellectual property
 # and proprietary rights in and to this software, related documentation
 # and any modifications thereto.  Any use, reproduction, disclosure or
 # distribution of this software and related documentation without an express
 # license agreement from NVIDIA CORPORATION is strictly prohibited.
 **************************************************************************/

#pragma pack_matrix(row_major)

#define RTXDI_ENABLE_PRESAMPLING 0
#include "RtxdiApplicationBridge/RtxdiApplicationBridge.hlsli"

#include <Rtxdi/DI/InitialSampling.hlsli>
#include <Rtxdi/DI/ReservoirStorage.hlsli>

[numthreads(RTXDI_SCREEN_SPACE_GROUP_SIZE, RTXDI_SCREEN_SPACE_GROUP_SIZE, 1)]
void main(uint2 pixelPosition : SV_DispatchThreadID)
{
    if (any(pixelPosition >= g_Const.view.viewportSize))
    {
        return;
    }

    const RTXDI_LightBufferParameters lightBufferParams = g_Const.lightBufferParams;

    // Use the same packed surface representation that the remaining passes will load.
    RAB_Surface surface = RAB_GetGBufferSurface(pixelPosition, false);

    RTXDI_DIReservoir reservoir = RTXDI_EmptyDIReservoir();

    if (RAB_IsSurfaceValid(surface))
    {
        // Initialize the RNG
        RTXDI_RandomSamplerState rng = RTXDI_InitRandomSampler(pixelPosition, g_Const.runtimeParams.frameIndex, 1);

        RTXDI_InitialSamplingMisData misData = RTXDI_ComputeInitialSamplingMisData(g_Const.initialSamplingParams);

        // Generate the initial sample
        RAB_LightSample lightSample = RAB_EmptyLightSample();
        RTXDI_DIReservoir localReservoir = RTXDI_SampleLocalLights(rng, rng, surface,
            g_Const.initialSamplingParams, misData, ReSTIRDI_LocalLightSamplingMode_UNIFORM, lightBufferParams.localLightBufferRegion, lightSample);
        RTXDI_CombineDIReservoirs(reservoir, localReservoir, 0.5, localReservoir.targetPdf);

        // Resample BRDF samples.
        RAB_LightSample brdfSample = RAB_EmptyLightSample();
        RTXDI_DIReservoir brdfReservoir = RTXDI_SampleBrdf(rng, surface, g_Const.initialSamplingParams.numBrdfSamples, g_Const.initialSamplingParams.brdfCutoff, 0.001f, misData, rng, lightBufferParams, brdfSample);
        bool selectBrdf = RTXDI_CombineDIReservoirs(reservoir, brdfReservoir, RTXDI_GetNextRandom(rng), brdfReservoir.targetPdf);
        if (selectBrdf)
        {
            lightSample = brdfSample;
        }

        RTXDI_FinalizeResampling(reservoir, 1.0, 1.0);
        reservoir.M = 1;

        // BRDF was generated with a trace so no need to trace visibility again
        if (RTXDI_IsValidDIReservoir(reservoir) && !selectBrdf)
        {
            // See if the initial sample is visible from the surface
            if (!RAB_GetConservativeVisibility(surface, lightSample))
            {
                // If not visible, discard the sample (but keep the M)
                RTXDI_StoreVisibilityInDIReservoir(reservoir, 0, true);
            }
        }
    }

    RTXDI_StoreDIReservoir(reservoir, g_Const.restirDIReservoirBufferParams, pixelPosition, g_Const.bufferIndices.initialSamplingOutputBufferIndex);
}
