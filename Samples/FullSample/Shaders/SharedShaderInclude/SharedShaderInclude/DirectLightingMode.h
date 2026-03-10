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

#ifndef DIRECT_LIGHTING_MODE_H
#define DIRECT_LIGHTING_MODE_H

#define DIRECT_LIGHTING_MODE_NONE 0
#define DIRECT_LIGHTING_MODE_BRDF 1
#define DIRECT_LIGHTING_MODE_RESTIR 2

#ifdef __cplusplus
enum class DirectLightingMode : uint32_t
{
    None = DIRECT_LIGHTING_MODE_NONE,
    Brdf = DIRECT_LIGHTING_MODE_BRDF,
    ReStir = DIRECT_LIGHTING_MODE_RESTIR
};
#else
#define DirectLightingMode uint
#endif

#endif // DIRECT_LIGHTING_MODE_H