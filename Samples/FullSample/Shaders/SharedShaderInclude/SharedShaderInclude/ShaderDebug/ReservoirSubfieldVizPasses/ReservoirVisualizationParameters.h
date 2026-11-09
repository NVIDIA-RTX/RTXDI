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

#ifndef RESERVOIR_VISUALIZATION_PARAMETERS_H
#define RESERVOIR_VISUALIZATION_PARAMETERS_H

#define RESERVOIR_VIZ_DISPLAY_VALUES 0
#define RESERVOIR_VIZ_DISPLAY_STATUS 1

#define RESERVOIR_VIZ_SCALAR_LINEAR 0
#define RESERVOIR_VIZ_SCALAR_LOG2 1
#define RESERVOIR_VIZ_SCALAR_LOG2_ONE_PLUS 2

// vectorVizMode values, interpreted by the selected field.
#define RESERVOIR_VIZ_UV_RG_COORDINATES 0
#define RESERVOIR_VIZ_UV_U_GRAYSCALE 1
#define RESERVOIR_VIZ_UV_V_GRAYSCALE 2

#define RESERVOIR_VIZ_POSITION_XYZ 0
#define RESERVOIR_VIZ_POSITION_X 1
#define RESERVOIR_VIZ_POSITION_Y 2
#define RESERVOIR_VIZ_POSITION_Z 3
#define RESERVOIR_VIZ_POSITION_DISTANCE 4

#define RESERVOIR_VIZ_NORMAL_RGB 0
#define RESERVOIR_VIZ_NORMAL_X 1
#define RESERVOIR_VIZ_NORMAL_Y 2
#define RESERVOIR_VIZ_NORMAL_Z 3

#define RESERVOIR_VIZ_DISPLACEMENT_MAGNITUDE 0
#define RESERVOIR_VIZ_DISPLACEMENT_DIRECTION_AND_MAGNITUDE 1

#define RESERVOIR_VIZ_LIGHT_ID_HASH 0
#define RESERVOIR_VIZ_LIGHT_ID_MATCH_REFERENCE 1

#define RESERVOIR_VIZ_RNG_SEED_HASH 0
#define RESERVOIR_VIZ_RNG_SEED_INDEX_HASH 1
#define RESERVOIR_VIZ_RNG_INDEX 2

#define RESERVOIR_VIZ_PT_RADIANCE_RGB 0
#define RESERVOIR_VIZ_PT_RADIANCE_LUMINANCE 1
#define RESERVOIR_VIZ_PT_RADIANCE_LIGHT_ID 2
#define RESERVOIR_VIZ_PT_RADIANCE_LIGHT_UV 3
#define RESERVOIR_VIZ_PT_RADIANCE_PAYLOAD_TYPE 4

#define RESERVOIR_VIZ_STATUS_VALID 0
#define RESERVOIR_VIZ_STATUS_INVALID 1
#define RESERVOIR_VIZ_STATUS_BELOW_RANGE 2
#define RESERVOIR_VIZ_STATUS_ABOVE_RANGE 3
#define RESERVOIR_VIZ_STATUS_OUTSIDE_BOTH 4
#define RESERVOIR_VIZ_STATUS_EMPTY 5
#define RESERVOIR_VIZ_STATUS_UNAVAILABLE 6
#define RESERVOIR_VIZ_STATUS_INACTIVE 7
#define RESERVOIR_VIZ_STATUS_COUNT 8

// Display-referred RGB; the shader and UI convert these values to linear RGB.
#ifdef __cplusplus
static const donut::math::float3 reservoirVizStatusColors[RESERVOIR_VIZ_STATUS_COUNT] = {
#else
static const float3 reservoirVizStatusColors[RESERVOIR_VIZ_STATUS_COUNT] = {
#endif
    { 0.f, 0.f, 0.f },       // Valid
    { 1.f, 0.f, 1.f },       // Invalid
    { .15f, .35f, 1.f },     // Below range
    { 1.f, .5f, .05f },      // Above range
    { 1.f, .85f, .05f },     // Outside both
    { .65f, .08f, .08f },    // Empty
    { .1f, .85f, .9f },      // Unused
    { .08f, .08f, .08f }     // Inactive
};

struct ReservoirVisualizationParameters
{
#ifdef __cplusplus
    using uint = uint32_t;
    using float3 = donut::math::float3;
#endif

    // Range endpoints are always expressed in the field's original units.
    float scalarMin;
    float scalarMax;
    uint scalarVizFunction; // RESERVOIR_VIZ_SCALAR_* function.
    uint vectorVizMode; // Field-specific RESERVOIR_VIZ_UV/POSITION/NORMAL/DISPLACEMENT_* value.

    float3 positionCenter;
    float positionExtent; // Half extent of the position visualization volume, in world units.

    float exposure; // Radiance exposure, in stops.
    float displacementSaturationDistance; // Pixel distance that reaches full saturation.
    uint colorMode; // RGB or luminance; PT Radiance also uses the light and payload-type modes above.
    uint rngVizMode; // RESERVOIR_VIZ_RNG_* value.

    uint referenceLightIndex; // Reference light index for the DI light-match view.
    uint lightIdVizMode; // RESERVOIR_VIZ_LIGHT_ID_* value.
    uint displayMode; // Values or field Status.
    uint pad0;
};

#endif // RESERVOIR_VISUALIZATION_PARAMETERS_H
