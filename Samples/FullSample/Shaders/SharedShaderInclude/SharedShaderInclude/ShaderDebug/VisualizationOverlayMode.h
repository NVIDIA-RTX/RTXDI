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

#ifndef RTXDI_VISUALIZATION_OVERLAY_MODE_H
#define RTXDI_VISUALIZATION_OVERLAY_MODE_H

enum
#ifdef __cplusplus
class
#endif
VisualizationOverlayMode
{
    VISUALIZATION_OVERLAY_MODE_NONE,
    VISUALIZATION_OVERLAY_MODE_COMPOSITED_COLOR,
    VISUALIZATION_OVERLAY_MODE_RESOLVED_COLOR,
    VISUALIZATION_OVERLAY_MODE_DIFFUSE,
    VISUALIZATION_OVERLAY_MODE_SPECULAR,
    VISUALIZATION_OVERLAY_MODE_DENOISED_DIFFUSE,
    VISUALIZATION_OVERLAY_MODE_DENOISED_SPECULAR,
    VISUALIZATION_OVERLAY_MODE_RESERVOIR_WEIGHT,
    VISUALIZATION_OVERLAY_MODE_RESERVOIR_M,
    VISUALIZATION_OVERLAY_MODE_DIFFUSE_GRADIENT,
    VISUALIZATION_OVERLAY_MODE_SPECULAR_GRADIENT,
    VISUALIZATION_OVERLAY_MODE_DIFFUSE_CONFIDENCE,
    VISUALIZATION_OVERLAY_MODE_SPECULAR_CONFIDENCE,
    VISUALIZATION_OVERLAY_MODE_GI_WEIGHT,
    VISUALIZATION_OVERLAY_MODE_GI_M
};

#endif // RTXDI_VISUALIZATION_OVERLAY_MODE_H