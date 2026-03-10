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

#ifndef GI_RESERVOIR_VIZ_PARAMETERS_H
#define GI_RESERVOIR_VIZ_PARAMETERS_H

#include <donut/shaders/view_cb.h>
#include <Rtxdi/RtxdiParameters.h>
#include <Rtxdi/GI/ReSTIRGIParameters.h>

enum
#ifdef __cplusplus
class
#endif
GIReservoirField
{
    GI_RESERVOIR_FIELD_POSITION,
    GI_RESERVOIR_FIELD_NORMAL,
    GI_RESERVOIR_FIELD_RADIANCE,
    GI_RESERVOIR_FIELD_WEIGHT_SUM,
    GI_RESERVOIR_FIELD_M,
    GI_RESERVOIR_FIELD_AGE
};

struct GIReservoirVizParameters
{
#ifdef __cplusplus
    using int2 = int[2];
    using float2 = float[2];
    using uint = uint32_t;
#endif

    PlanarViewConstants view;
    RTXDI_RuntimeParameters runtimeParams;

    RTXDI_ReservoirBufferParameters reservoirBufferParams;
    RTXDI_GIBufferIndices bufferIndices;

	GIReservoirField giReservoirField;
    uint pad1;
    uint pad2;
    uint pad3;
};

#endif // GI_RESERVOIR_VIZ_PARAMETERS_H