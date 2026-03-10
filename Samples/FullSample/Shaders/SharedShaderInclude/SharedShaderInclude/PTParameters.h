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

#ifndef PT_PARAMETERS_H
#define PT_PARAMETERS_H

#include <Rtxdi/DI/ReSTIRDIParameters.h>

// MIS between light from emissive surfaces vs NEE
#define PT_INITIAL_SAMPLING_LIGHT_SAMPLING_MODE_EMISSIVE_ONLY 0
#define PT_INITIAL_SAMPLING_LIGHT_SAMPLING_MODE_NEE_ONLY 1
#define PT_INITIAL_SAMPLING_LIGHT_SAMPLING_MODE_MIS 2

#ifdef __cplusplus
enum class PTInitialSamplingLightSamplingMode : uint32_t
{
    EmissiveOnly = PT_INITIAL_SAMPLING_LIGHT_SAMPLING_MODE_EMISSIVE_ONLY,
    NeeOnly = PT_INITIAL_SAMPLING_LIGHT_SAMPLING_MODE_NEE_ONLY,
    Mis = PT_INITIAL_SAMPLING_LIGHT_SAMPLING_MODE_MIS
};
#else
#define PTInitialSamplingLightSamplingMode uint32_t
#endif

struct PTNeeParameters
{
	RTXDI_DIInitialSamplingParameters initialSamplingParams;
	RTXDI_DISpatialResamplingParameters spatialResamplingParams;
};

struct PTParameters
{
	uint32_t enableRussianRoulette;
	float    russianRouletteContinueChance;
    uint32_t enableSecondaryDISpatialResampling;
    uint32_t copyReSTIRDISimilarityThresholds;

	PTNeeParameters nee;

    uint32_t sampleEnvMapOnSecondaryMiss;
    uint32_t sampleEmissivesOnSecondaryHit;
    PTInitialSamplingLightSamplingMode lightSamplingMode;
    uint32_t extraMirrorBounceBudget;

    float minimumPathThroughput;
    uint32_t pad1;
    uint32_t pad2;
    uint32_t pad3;
};

#endif // PT_PARAMETERS_H