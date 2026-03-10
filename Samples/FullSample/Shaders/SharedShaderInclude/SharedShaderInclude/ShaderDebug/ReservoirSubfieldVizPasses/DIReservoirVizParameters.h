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

#ifndef DI_RESERVOIR_VIZ_PARAMETERS_H
#define DI_RESERVOIR_VIZ_PARAMETERS_H

#include <donut/shaders/view_cb.h>
#include <Rtxdi/RtxdiParameters.h>
#include <Rtxdi/DI/ReSTIRDIParameters.h>

enum
#ifdef __cplusplus
class
#endif
DIReservoirField
{
    DI_RESERVOIR_FIELD_LIGHT_DATA,
    DI_RESERVOIR_FIELD_UV_DATA,
    DI_RESERVOIR_FIELD_TARGET_PDF,
    DI_RESERVOIR_FIELD_M,
    DI_RESERVOIR_FIELD_PACKED_VISIBILITY,
    DI_RESERVOIR_FIELD_SPATIAL_DISTANCE,
    DI_RESERVOIR_FIELD_AGE,
    DI_RESERVOIR_FIELD_CANONICAL_WEIGHT
};

struct DIReservoirVizParameters
{
#ifdef __cplusplus
    using int2 = int[2];
    using float2 = float[2];
    using uint = uint32_t;
#endif

    PlanarViewConstants view;
    RTXDI_RuntimeParameters runtimeParams;

    RTXDI_ReservoirBufferParameters reservoirBufferParams;
    RTXDI_DIBufferIndices bufferIndices;

	DIReservoirField diReservoirField;
    uint maxLightsInBuffer;
    uint pad1;
    uint pad2;
};

#endif // DI_RESERVOIR_VIZ_PARAMETERS_H