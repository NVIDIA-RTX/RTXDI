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
 * Visualize PT duplication map: R32_UINT count (max 288) as grayscale.
 */

Texture2D<uint> t_DuplicationMap : register(t0);
RWTexture2D<float4> t_Output : register(u0);

static const float c_maxCount = 288.0;

[numthreads(16, 16, 1)]
void main(uint2 pixelPosition : SV_DispatchThreadID)
{
    uint count = t_DuplicationMap[pixelPosition];
    float n = saturate(float(count) / c_maxCount);
    t_Output[pixelPosition] = float4(n, n, n, 1.0);
}
