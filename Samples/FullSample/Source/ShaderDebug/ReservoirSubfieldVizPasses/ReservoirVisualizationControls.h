/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 */

#pragma once

#include <array>
#include <cstdint>
#include <donut/core/math/math.h>
#include "SharedShaderInclude/ShaderDebug/ReservoirSubfieldVizPasses/ReservoirVisualizationParameters.h"

struct UIData;
struct UIDebugOutputSettings;

struct ReservoirVisualizationStatistics
{
    std::array<uint32_t, RESERVOIR_VIZ_STATUS_COUNT> counts = {};
    ReservoirVisualizationParameters parameters = {};
    uint32_t fieldIndex = 0;
    uint32_t frameIndex = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    bool available = false;
};

struct ReservoirVisualizationSettings
{
    // Preserve independent ranges and display choices for every existing subfield.
    std::array<ReservoirVisualizationParameters, 27> fields;
    std::array<bool, 27> positionBoundsInitialized = {};
    ReservoirVisualizationStatistics statistics;
};

ReservoirVisualizationSettings CreateReservoirVisualizationSettings();
uint32_t GetReservoirVisualizationFieldIndex(const UIDebugOutputSettings& settings);
const ReservoirVisualizationParameters& GetReservoirVisualizationParameters(const UIDebugOutputSettings& settings);
bool ReservoirVisualizationParametersMatch(const ReservoirVisualizationParameters& left, const ReservoirVisualizationParameters& right);
void DrawReservoirVisualizationControls(UIData& ui);
