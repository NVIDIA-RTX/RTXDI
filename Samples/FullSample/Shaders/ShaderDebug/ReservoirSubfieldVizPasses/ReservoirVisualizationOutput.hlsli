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

#ifndef RESERVOIR_VISUALIZATION_OUTPUT_HLSLI
#define RESERVOIR_VISUALIZATION_OUTPUT_HLSLI

// Included after the pass declares t_Output and u_VisualizationCounts.
// The host clears the counters before each dispatch and reads them asynchronously.
groupshared uint g_VisualizationGroupCounts[RESERVOIR_VIZ_STATUS_COUNT];

void WriteReservoirVisualization(ReservoirVisualizationResult result, uint2 pixelPosition,
    uint groupIndex, bool insideViewport, ReservoirVisualizationParameters settings)
{
    if (groupIndex < RESERVOIR_VIZ_STATUS_COUNT)
    {
        g_VisualizationGroupCounts[groupIndex] = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    if (insideViewport)
    {
        InterlockedAdd(g_VisualizationGroupCounts[result.status], 1);
        t_Output[pixelPosition] = ResolveReservoirVisualization(result, settings);
    }
    GroupMemoryBarrierWithGroupSync();

    if (groupIndex < RESERVOIR_VIZ_STATUS_COUNT && g_VisualizationGroupCounts[groupIndex] != 0)
    {
        InterlockedAdd(u_VisualizationCounts[groupIndex], g_VisualizationGroupCounts[groupIndex]);
    }
}

#endif // RESERVOIR_VISUALIZATION_OUTPUT_HLSLI
