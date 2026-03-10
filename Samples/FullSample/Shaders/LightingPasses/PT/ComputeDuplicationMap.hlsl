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

/*
 * Compute duplication map: for each pixel count how many neighbors (17x17)
 * share the same sample ID (from FillSampleID), cap at 288. Used for
 * duplication-based temporal history reduction.
 */

#pragma pack_matrix(row_major)

#include "../RtxdiApplicationBridge/RtxdiApplicationBridge.hlsli"

#define DUPMAP_TILE 32
#define DUPMAP_RAD 8
#define DUPMAP_MAX_COUNT 288u

groupshared uint s_sampleIDs[DUPMAP_TILE][DUPMAP_TILE];

[numthreads(16, 16, 1)]
void main(uint2 GroupID : SV_GroupID, uint2 ThreadID : SV_GroupThreadID)
{
    const int2 frameDim = int2(g_Const.view.viewportSize);
    const int2 base = int2(GroupID) * 16 - DUPMAP_RAD;

    // Load 32x32 tile into LDS (each of 16x16 threads loads 2x2)
    for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx)
        {
            int2 readPixel = base + int2(ThreadID.x * 2 + dx, ThreadID.y * 2 + dy);
            uint id = 0u;
            if (all(readPixel >= 0) && all(readPixel < frameDim))
                id = u_PTSampleIDTexture[readPixel];
            s_sampleIDs[ThreadID.x * 2 + dx][ThreadID.y * 2 + dy] = id;
        }
    GroupMemoryBarrierWithGroupSync();

    int2 pixel = base + int2(ThreadID.x + DUPMAP_RAD, ThreadID.y + DUPMAP_RAD);
    if (any(pixel < 0) || any(pixel >= frameDim))
        return;

    uint ownSample = s_sampleIDs[ThreadID.x + DUPMAP_RAD][ThreadID.y + DUPMAP_RAD];
    uint count = 0u;
    [loop]
    for (int ny = -DUPMAP_RAD; ny <= DUPMAP_RAD && count < DUPMAP_MAX_COUNT; ++ny)
        [loop]
        for (int nx = -DUPMAP_RAD; nx <= DUPMAP_RAD && count < DUPMAP_MAX_COUNT; ++nx)
        {
            if (nx == 0 && ny == 0)
                continue;
            if (s_sampleIDs[ThreadID.x + DUPMAP_RAD + nx][ThreadID.y + DUPMAP_RAD + ny] == ownSample && ownSample != 0u)
                ++count;
        }
    u_PTDuplicationMap[pixel] = min(count, DUPMAP_MAX_COUNT);
}
