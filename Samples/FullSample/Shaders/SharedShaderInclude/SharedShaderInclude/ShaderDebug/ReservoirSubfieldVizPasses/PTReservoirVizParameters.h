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

#ifndef PT_RESERVOIR_VIZ_PARAMETERS_H
#define PT_RESERVOIR_VIZ_PARAMETERS_H

#include <donut/shaders/view_cb.h>
#include <Rtxdi/RtxdiParameters.h>
#include <Rtxdi/PT/ReSTIRPTParameters.h>

enum
#ifdef __cplusplus
class
#endif
PTReservoirField
{
    PT_RESERVOIR_FIELD_TRANSLATED_WORLD_POSITION,
    PT_RESERVOIR_FIELD_WEIGHT_SUM,
    PT_RESERVOIR_FIELD_WORLD_NORMAL,
    PT_RESERVOIR_FIELD_M,
    PT_RESERVOIR_FIELD_RADIANCE,
    PT_RESERVOIR_FIELD_AGE,
    PT_RESERVOIR_FIELD_RC_WI_PDF,
    PT_RESERVOIR_FIELD_PARTIAL_JACOBIAN,
    PT_RESERVOIR_FIELD_RC_VERTEX_LENGTH,
    PT_RESERVOIR_FIELD_PATH_LENGTH,
    PT_RESERVOIR_FIELD_RANDOM_SEED,
    PT_RESERVOIR_FIELD_RANDOM_INDEX,
    PT_RESERVOIR_FIELD_TARGET_FUNCTION
};

struct PTReservoirVizParameters
{
#ifdef __cplusplus
    using int2 = int[2];
    using float2 = float[2];
    using uint = uint32_t;
#endif

    PlanarViewConstants view;
    RTXDI_RuntimeParameters runtimeParams;

    RTXDI_ReservoirBufferParameters reservoirBufferParams;
    RTXDI_PTBufferIndices bufferIndices;

	PTReservoirField ptReservoirField;
    uint pad1;
    uint pad2;
    uint pad3;
};

#endif // PT_RESERVOIR_VIZ_PARAMETERS_H