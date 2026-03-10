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

#ifndef RTXDI_RAB_LIGHT_INFO_HLSLI
#define RTXDI_RAB_LIGHT_INFO_HLSLI

struct RAB_LightSample
{
    float3 position;
    float3 normal;
    float3 radiance;
    float solidAnglePdf;
    PolymorphicLightType lightType;
};

RAB_LightSample RAB_EmptyLightSample()
{
    return (RAB_LightSample)0;
}

float3 RAB_LightSamplePosition(RAB_LightSample lightSample)
{
	return lightSample.position;
}

bool RAB_IsAnalyticLightSample(RAB_LightSample lightSample)
{
    return lightSample.lightType != PolymorphicLightType::kTriangle && 
        lightSample.lightType != PolymorphicLightType::kEnvironment;
}

float RAB_LightSampleSolidAnglePdf(RAB_LightSample lightSample)
{
    return lightSample.solidAnglePdf;
}

float3 RAB_LightSampleRadiance(RAB_LightSample lightSample)
{
    return lightSample.radiance;
}

#endif // RTXDI_RAB_LIGHT_INFO_HLSLI