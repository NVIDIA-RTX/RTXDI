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

#ifndef PT_PATH_VIZ_CONSTANTS_H
#define PT_PATH_VIZ_CONSTANTS_H

#include "donut/shaders/view_cb.h"

struct PTPathVizPreprocessConstants
{
    float normalVectorVizLength;
    uint maxVerticesPerPath;
    uint2 pad;
};

struct PTPathVizLineData
{
    float3 color;
    uint32_t enabled;
};

struct PTPathVizPaths
{
    PTPathVizLineData initial;
    PTPathVizLineData temporalRetrace;
    PTPathVizLineData temporalShift;
    PTPathVizLineData temporalInverseShift;

    PTPathVizLineData spatialRetrace;
    PTPathVizLineData spatialShift;
    PTPathVizLineData spatialInverseShift;
    PTPathVizLineData normal;

    PTPathVizLineData nee;
};

struct PTPathVizLineConstants
{
    PlanarViewConstants view;

    PTPathVizPaths paths;

    uint enableCameraVertex;
    uint maxVerticesPerPath;
    uint2 pad;
};

#endif // PT_PATH_VIZ_CONSTANTS_H