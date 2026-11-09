/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 */

#include "ReservoirVisualizationControls.h"

#include "UserInterface.h"
#include "Scene/SampleScene.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
    enum class FieldKind
    {
        Light, UV, Scalar, Count, IntegerCount, Visibility, Displacement, Age, Unavailable,
        Position, Normal, Radiance, PTRadiance, Seed, Index, Target, PartialJacobian, GIWeight
    };

    struct FieldDescription
    {
        const char* label;
        const char* explanation;
        FieldKind kind;
        float minimum;
        float maximum;
        uint32_t scalarVizFunction;
    };

    struct VisualizationModeChoice
    {
        const char* label;
        uint32_t value;
    };

    constexpr VisualizationModeChoice c_ScalarFunctionChoices[] = {
        { "Linear", RESERVOIR_VIZ_SCALAR_LINEAR },
        { "Log2", RESERVOIR_VIZ_SCALAR_LOG2 },
        { "Log2(1 + value)", RESERVOIR_VIZ_SCALAR_LOG2_ONE_PLUS }
    };

    constexpr VisualizationModeChoice c_ZeroInclusiveScalarFunctionChoices[] = {
        { "Linear", RESERVOIR_VIZ_SCALAR_LINEAR },
        { "Log2(1 + value)", RESERVOIR_VIZ_SCALAR_LOG2_ONE_PLUS }
    };

    constexpr VisualizationModeChoice c_UVChoices[] = {
        { "RG coordinates", RESERVOIR_VIZ_UV_RG_COORDINATES },
        { "U grayscale", RESERVOIR_VIZ_UV_U_GRAYSCALE },
        { "V grayscale", RESERVOIR_VIZ_UV_V_GRAYSCALE }
    };

    constexpr VisualizationModeChoice c_PositionChoices[] = {
        { "XYZ coordinates", RESERVOIR_VIZ_POSITION_XYZ },
        { "X offset", RESERVOIR_VIZ_POSITION_X },
        { "Y offset", RESERVOIR_VIZ_POSITION_Y },
        { "Z offset", RESERVOIR_VIZ_POSITION_Z },
        { "Distance from center", RESERVOIR_VIZ_POSITION_DISTANCE }
    };

    constexpr VisualizationModeChoice c_NormalChoices[] = {
        { "Directional RGB", RESERVOIR_VIZ_NORMAL_RGB },
        { "X component", RESERVOIR_VIZ_NORMAL_X },
        { "Y component", RESERVOIR_VIZ_NORMAL_Y },
        { "Z component", RESERVOIR_VIZ_NORMAL_Z }
    };

    constexpr VisualizationModeChoice c_DisplacementChoices[] = {
        { "Magnitude", RESERVOIR_VIZ_DISPLACEMENT_MAGNITUDE },
        { "Direction and magnitude", RESERVOIR_VIZ_DISPLACEMENT_DIRECTION_AND_MAGNITUDE }
    };

    constexpr VisualizationModeChoice c_LightIdChoices[] = {
        { "Hash colors", RESERVOIR_VIZ_LIGHT_ID_HASH },
        { "Match index", RESERVOIR_VIZ_LIGHT_ID_MATCH_REFERENCE }
    };

    constexpr VisualizationModeChoice c_RngSeedChoices[] = {
        { "Seed hash", RESERVOIR_VIZ_RNG_SEED_HASH },
        { "Seed + index hash", RESERVOIR_VIZ_RNG_SEED_INDEX_HASH }
    };

    constexpr VisualizationModeChoice c_RngIndexChoices[] = {
        { "Index", RESERVOIR_VIZ_RNG_INDEX },
        { "Seed + index hash", RESERVOIR_VIZ_RNG_SEED_INDEX_HASH }
    };

    constexpr size_t c_GIFieldOffset = 8;
    constexpr size_t c_PTFieldOffset = 14;
    static_assert(size_t(DIReservoirField::DI_RESERVOIR_FIELD_CANONICAL_WEIGHT) + 1 == c_GIFieldOffset);
    static_assert(size_t(GIReservoirField::GI_RESERVOIR_FIELD_AGE) + 1 == c_PTFieldOffset - c_GIFieldOffset);
    static_assert(size_t(PTReservoirField::PT_RESERVOIR_FIELD_TARGET_FUNCTION) + 1 == 27 - c_PTFieldOffset);

    const std::array<FieldDescription, 27> c_Fields = {{
        { "Selected light", nullptr, FieldKind::Light, 0.f, 1.f, RESERVOIR_VIZ_SCALAR_LINEAR },
        { "Light sample UV", nullptr, FieldKind::UV, 0.f, 1.f, RESERVOIR_VIZ_SCALAR_LINEAR },
        { "Target density / importance", "Unnormalized target function.", FieldKind::Scalar, 1e-10f, 1.f, RESERVOIR_VIZ_SCALAR_LOG2 },
        { "Represented candidate count M", nullptr, FieldKind::IntegerCount, 0.f, 32.f, RESERVOIR_VIZ_SCALAR_LINEAR },
        { "Cached visibility", nullptr, FieldKind::Visibility, 0.f, 1.f, RESERVOIR_VIZ_SCALAR_LINEAR },
        { "Visibility displacement (pixels)", "Offset from the visibility origin, corrected for temporal motion.", FieldKind::Displacement, 0.f, 32.f, RESERVOIR_VIZ_SCALAR_LINEAR },
        { "Visibility age (frames)", "Frames since visibility evaluation.", FieldKind::Age, 0.f, 16.f, RESERVOIR_VIZ_SCALAR_LINEAR },
        { "Canonical weight", "Pairwise MIS weight.", FieldKind::Unavailable, 0.f, 1.f, RESERVOIR_VIZ_SCALAR_LINEAR },
        { "Selected secondary position (world units)", nullptr, FieldKind::Position, 0.f, 40.f, RESERVOIR_VIZ_SCALAR_LINEAR },
        { "Selected secondary normal (world space)", nullptr, FieldKind::Normal, 0.f, 1.f, RESERVOIR_VIZ_SCALAR_LINEAR },
        { "Selected sample radiance", "Incoming radiance at the selected secondary surface.", FieldKind::Radiance, 1e-4f, 100.f, RESERVOIR_VIZ_SCALAR_LOG2 },
        { "Finalized reservoir weight", "Normalized weight after resampling.", FieldKind::GIWeight, 0.f, 10.f, RESERVOIR_VIZ_SCALAR_LOG2_ONE_PLUS },
        { "Represented candidate count M", nullptr, FieldKind::IntegerCount, 0.f, 32.f, RESERVOIR_VIZ_SCALAR_LINEAR },
        { "Selected sample age (frames)", nullptr, FieldKind::Age, 0.f, 30.f, RESERVOIR_VIZ_SCALAR_LINEAR },
        { "Stored position (world units)", nullptr, FieldKind::Position, 0.f, 40.f, RESERVOIR_VIZ_SCALAR_LINEAR },
        { "Finalized reservoir weight", "Normalized weight after resampling.", FieldKind::Scalar, 1e-3f, 1000.f, RESERVOIR_VIZ_SCALAR_LOG2 },
        { "Stored normal (world space)", nullptr, FieldKind::Normal, 0.f, 1.f, RESERVOIR_VIZ_SCALAR_LINEAR },
        { "Represented candidate count M", "Effective candidate count; supports fractional values.", FieldKind::Count, 0.f, 64.f, RESERVOIR_VIZ_SCALAR_LOG2_ONE_PLUS },
        { "Radiance / light data", nullptr, FieldKind::PTRadiance, 1e-4f, 100.f, RESERVOIR_VIZ_SCALAR_LOG2 },
        { "PT age (frames)", nullptr, FieldKind::Age, 0.f, 31.f, RESERVOIR_VIZ_SCALAR_LINEAR },
        { "Reconnection outgoing BSDF PDF", nullptr, FieldKind::Scalar, 1e-4f, 100.f, RESERVOIR_VIZ_SCALAR_LOG2 },
        { "Partial Jacobian", "Inverse geometry (world units squared) or source-light solid-angle PDF.", FieldKind::PartialJacobian, 0.f, 1000.f, RESERVOIR_VIZ_SCALAR_LOG2_ONE_PLUS },
        { "Reconnection depth (vertices)", nullptr, FieldKind::IntegerCount, 0.f, 8.f, RESERVOIR_VIZ_SCALAR_LINEAR },
        { "Path length (segments)", "Segments from the primary surface to the final light.", FieldKind::IntegerCount, 0.f, 8.f, RESERVOIR_VIZ_SCALAR_LINEAR },
        { "Random seed", nullptr, FieldKind::Seed, 0.f, 1.f, RESERVOIR_VIZ_SCALAR_LINEAR },
        { "Saved random-stream index", nullptr, FieldKind::Index, 0.f, 255.f, RESERVOIR_VIZ_SCALAR_LINEAR },
        { "Target function / importance", "Luminance drives RIS selection.", FieldKind::Target, 1e-4f, 100.f, RESERVOIR_VIZ_SCALAR_LOG2 }
    }};

    size_t GetFieldIndex(const UIDebugOutputSettings& settings)
    {
        switch (settings.reservoirSubfieldVizMode)
        {
        case ReservoirSubfieldVizMode::DIReservoir:
            return std::min(size_t(settings.diReservoirVizField), c_GIFieldOffset - 1);
        case ReservoirSubfieldVizMode::GIReservoir:
            return c_GIFieldOffset + std::min(size_t(settings.giReservoirVizField), c_PTFieldOffset - c_GIFieldOffset - 1);
        default:
            return c_PTFieldOffset + std::min(size_t(settings.ptReservoirVizField), c_Fields.size() - c_PTFieldOffset - 1);
        }
    }

    ReservoirVisualizationParameters MakeParameters(const FieldDescription& field)
    {
        ReservoirVisualizationParameters parameters = {};
        parameters.positionCenter = float3(0.f);
        parameters.scalarMin = field.minimum;
        parameters.scalarMax = field.maximum;
        parameters.scalarVizFunction = field.scalarVizFunction;
        parameters.positionExtent = 20.f;
        parameters.displacementSaturationDistance = 32.f;
        switch (field.kind)
        {
        case FieldKind::UV:
            parameters.vectorVizMode = RESERVOIR_VIZ_UV_RG_COORDINATES;
            break;
        case FieldKind::Position:
            parameters.vectorVizMode = RESERVOIR_VIZ_POSITION_XYZ;
            break;
        case FieldKind::Normal:
            parameters.vectorVizMode = RESERVOIR_VIZ_NORMAL_RGB;
            break;
        case FieldKind::Displacement:
            parameters.vectorVizMode = RESERVOIR_VIZ_DISPLACEMENT_DIRECTION_AND_MAGNITUDE;
            break;
        default:
            break;
        }
        parameters.lightIdVizMode = RESERVOIR_VIZ_LIGHT_ID_HASH;
        parameters.rngVizMode = field.kind == FieldKind::Index ? RESERVOIR_VIZ_RNG_INDEX : RESERVOIR_VIZ_RNG_SEED_HASH;
        parameters.colorMode = 0;
        parameters.displayMode = RESERVOIR_VIZ_DISPLAY_VALUES;
        return parameters;
    }

    bool EditChoice(const char* label, uint32_t& value, const char* choices)
    {
        int choice = int(value);
        if (ImGui::Combo(label, &choice, choices))
        {
            value = uint32_t(choice);
            return true;
        }
        return false;
    }

    template<size_t N>
    bool EditVisualizationMode(const char* label, uint32_t& value, const VisualizationModeChoice (&choices)[N])
    {
        const char* labels[N];
        int selected = -1;
        for (size_t index = 0; index < N; ++index)
        {
            labels[index] = choices[index].label;
            if (choices[index].value == value)
            {
                selected = int(index);
            }
        }
        if (ImGui::Combo(label, &selected, labels, int(N)))
        {
            value = choices[selected].value;
            return true;
        }
        return false;
    }

    float ToLinear(float value)
    {
        return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
    }

    struct StatusDescription
    {
        const char* label;
        const char* explanation;
    };

    static_assert(RESERVOIR_VIZ_STATUS_INACTIVE + 1 == RESERVOIR_VIZ_STATUS_COUNT);
    const std::array<StatusDescription, RESERVOIR_VIZ_STATUS_INACTIVE> c_Statuses = {{
        { "Valid", "In range." },
        { "Invalid", "Nonfinite value, invalid field data, or invalid settings." },
        { "Below range", "Below minimum." },
        { "Above range", "Above maximum." },
        { "Outside both", "Components below and above bounds." },
        { "Empty", nullptr },
        { "Unused", "This field or view does not apply to this reservoir." }
    }};

    void DrawDisplayModeControls(ReservoirVisualizationParameters& parameters)
    {
        if (parameters.displayMode != RESERVOIR_VIZ_DISPLAY_STATUS)
        {
            parameters.displayMode = RESERVOIR_VIZ_DISPLAY_VALUES;
        }
        if (ImGui::RadioButton("Values", parameters.displayMode == RESERVOIR_VIZ_DISPLAY_VALUES))
        {
            parameters.displayMode = RESERVOIR_VIZ_DISPLAY_VALUES;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Status", parameters.displayMode == RESERVOIR_VIZ_DISPLAY_STATUS))
        {
            parameters.displayMode = RESERVOIR_VIZ_DISPLAY_STATUS;
        }
    }

    void DrawStatusStatistics(const ReservoirVisualizationParameters& parameters,
        const ReservoirVisualizationStatistics& statistics, uint32_t fieldIndex)
    {
        const bool countsAvailable = statistics.available && statistics.fieldIndex == fieldIndex
            && ReservoirVisualizationParametersMatch(statistics.parameters, parameters)
            && statistics.width > 0 && statistics.height > 0;
        uint64_t activePixelCount = 0;
        for (size_t status = 0; status < c_Statuses.size(); ++status)
        {
            activePixelCount += statistics.counts[status];
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Active pixel status");
        if (countsAvailable)
        {
            ImGui::Text("Last completed frame: %u (%u x %u)", statistics.frameIndex, statistics.width, statistics.height);
        }
        else
        {
            ImGui::TextWrapped("Awaiting counts...");
        }

        if (ImGui::BeginTable("StatusCounts", 3, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Pixels", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 6.f);
            ImGui::TableSetupColumn("Percent", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 4.5f);
            ImGui::TableHeadersRow();
            for (uint32_t status = 0; status < uint32_t(c_Statuses.size()); ++status)
            {
                StatusDescription description = c_Statuses[status];
                if (status == RESERVOIR_VIZ_STATUS_UNAVAILABLE && c_Fields[fieldIndex].kind == FieldKind::PTRadiance)
                {
                    switch (parameters.colorMode)
                    {
                    case RESERVOIR_VIZ_PT_RADIANCE_RGB:
                    case RESERVOIR_VIZ_PT_RADIANCE_LUMINANCE:
                        description.label = "Light data";
                        description.explanation = "Stores a next-event-estimation light ID and UV instead of radiance.";
                        break;
                    case RESERVOIR_VIZ_PT_RADIANCE_LIGHT_ID:
                    case RESERVOIR_VIZ_PT_RADIANCE_LIGHT_UV:
                        description.label = "Radiance";
                        description.explanation = "Stores RGB radiance instead of a light ID and UV.";
                        break;
                    default:
                        description.label = "Alternate data";
                        description.explanation = "Stores the other payload type: RGB radiance or a light ID and UV.";
                        break;
                    }
                }
                ImGui::PushID(int(status));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                const float3 displayColor = reservoirVizStatusColors[status];
                const ImVec4 color(ToLinear(displayColor.x), ToLinear(displayColor.y), ToLinear(displayColor.z), 1.f);
                ImGui::ColorButton("##Color", color,
                    ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
                    ImVec2(ImGui::GetFontSize(), ImGui::GetFontSize()));
                if (description.explanation && ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", description.explanation);
                }
                ImGui::SameLine();
                ImGui::TextUnformatted(description.label);
                if (description.explanation && ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", description.explanation);
                }
                ImGui::TableNextColumn();
                if (countsAvailable)
                {
                    ImGui::Text("%u", statistics.counts[status]);
                }
                else
                {
                    ImGui::TextUnformatted("--");
                }
                ImGui::TableNextColumn();
                if (countsAvailable)
                {
                    const double percent = activePixelCount > 0
                        ? 100.0 * double(statistics.counts[status]) / double(activePixelCount) : 0.0;
                    ImGui::Text("%.2f%%", percent);
                }
                else
                {
                    ImGui::TextUnformatted("--");
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    void DrawScalarLegend(const ReservoirVisualizationParameters& parameters)
    {
        // Match the display-referred anchors in ReservoirVisualization.hlsli. Both the
        // debug output and ImGui vertex colors are linear before the sRGB framebuffer.
        const float3 colors[] = {
            { .267f, .005f, .329f }, { .279f, .175f, .483f }, { .230f, .322f, .546f },
            { .173f, .449f, .558f }, { .128f, .567f, .551f }, { .158f, .684f, .502f },
            { .369f, .789f, .383f }, { .678f, .864f, .190f }, { .993f, .906f, .144f }
        };
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        for (int i = 0; i < 64; ++i)
        {
            const float coordinate = float(i) * 8.f / 63.f;
            const int index = std::min(int(coordinate), 7);
            const float3 color = dm::lerp(colors[index], colors[index + 1], coordinate - float(index));
            const ImU32 packed = ImGui::ColorConvertFloat4ToU32(ImVec4(ToLinear(color.x), ToLinear(color.y), ToLinear(color.z), 1.f));
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(origin.x + width * float(i) / 64.f, origin.y),
                ImVec2(origin.x + width * float(i + 1) / 64.f, origin.y + 12.f), packed);
        }
        ImGui::Dummy(ImVec2(width, 12.f));
        float midpoint = (parameters.scalarMin + parameters.scalarMax) * .5f;
        if (parameters.scalarVizFunction == RESERVOIR_VIZ_SCALAR_LOG2)
        {
            midpoint = std::exp2((std::log2(parameters.scalarMin) + std::log2(parameters.scalarMax)) * .5f);
        }
        else if (parameters.scalarVizFunction == RESERVOIR_VIZ_SCALAR_LOG2_ONE_PLUS)
        {
            midpoint = float(std::expm1((std::log1p(double(parameters.scalarMin)) + std::log1p(double(parameters.scalarMax))) * .5));
        }
        ImGui::Text("Low %.5g   |   Mid %.5g   |   High %.5g", parameters.scalarMin, midpoint, parameters.scalarMax);
    }

    void DrawScalarControls(ReservoirVisualizationParameters& parameters, bool allowLog2 = true)
    {
        if (allowLog2)
        {
            EditVisualizationMode("Scalar function", parameters.scalarVizFunction, c_ScalarFunctionChoices);
        }
        else
        {
            if (parameters.scalarVizFunction == RESERVOIR_VIZ_SCALAR_LOG2)
            {
                parameters.scalarVizFunction = RESERVOIR_VIZ_SCALAR_LOG2_ONE_PLUS;
                parameters.scalarMin = 0.f;
            }
            else if (parameters.scalarVizFunction != RESERVOIR_VIZ_SCALAR_LINEAR
                && parameters.scalarVizFunction != RESERVOIR_VIZ_SCALAR_LOG2_ONE_PLUS)
            {
                parameters.scalarVizFunction = RESERVOIR_VIZ_SCALAR_LINEAR;
            }
            EditVisualizationMode("Scalar function", parameters.scalarVizFunction, c_ZeroInclusiveScalarFunctionChoices);
        }
        ImGui::InputFloat("Range minimum", &parameters.scalarMin, 0.f, 0.f, "%.6g");
        ImGui::InputFloat("Range maximum", &parameters.scalarMax, 0.f, 0.f, "%.6g");
        const float minimum = parameters.scalarVizFunction == RESERVOIR_VIZ_SCALAR_LOG2 ? 1e-20f : 0.f;
        parameters.scalarMin = std::isfinite(parameters.scalarMin) ? std::clamp(parameters.scalarMin, minimum, 1e20f) : minimum;
        parameters.scalarMax = std::isfinite(parameters.scalarMax) ? std::clamp(parameters.scalarMax, minimum, 1e21f) : 1.f;
        if (parameters.scalarMax <= parameters.scalarMin)
        {
            parameters.scalarMax = std::max(parameters.scalarMin * 2.f, parameters.scalarMin + 1.f);
        }
        DrawScalarLegend(parameters);
    }

    void FitPositionToScene(UIData& ui, ReservoirVisualizationParameters& parameters, float extentScale = 1.f)
    {
        if (!ui.resources->scene)
        {
            return;
        }
        const auto& bounds = ui.resources->scene->GetSceneGraph()->GetRootNode()->GetGlobalBoundingBox();
        if (bounds.isempty() || !dm::all(dm::isfinite(bounds.m_mins)) || !dm::all(dm::isfinite(bounds.m_maxs)))
        {
            return;
        }
        parameters.positionCenter = bounds.center();
        const float3 size = bounds.diagonal();
        parameters.positionExtent = std::max(std::max({ size.x, size.y, size.z }) * .5f * extentScale, .001f);
        parameters.scalarMin = 0.f;
        parameters.scalarMax = parameters.positionExtent * std::sqrt(3.f);
    }

    void DrawRngControls(ReservoirVisualizationParameters& parameters, bool isRandomIndex)
    {
        if (isRandomIndex)
        {
            EditVisualizationMode("RNG state display", parameters.rngVizMode, c_RngIndexChoices);
        }
        else
        {
            EditVisualizationMode("RNG state display", parameters.rngVizMode, c_RngSeedChoices);
        }
        if (isRandomIndex && parameters.rngVizMode == RESERVOIR_VIZ_RNG_INDEX)
        {
            DrawScalarControls(parameters, false);
        }
    }

    void DrawAgeLimit(UIData& ui, ReservoirVisualizationParameters& parameters)
    {
        uint32_t limit = 0;
        // Match the effective policy in SceneRenderer::UpdateReSTIRPTContextFromUI:
        // duplication tracking owns the shared age field even if rejection was requested.
        const bool ptAgeRejection = ui.restirPT.temporalResampling.enableAgeBasedRejection
            && !rtxdi::NeedsDuplicationMap(ui.restirPT.temporalResampling, ui.restirPT.decorrelation);
        switch (ui.debugOutputSettings.reservoirSubfieldVizMode)
        {
        case ReservoirSubfieldVizMode::DIReservoir:
            limit = ui.restirDI.shadingParams.finalVisibilityMaxAge;
            ImGui::Text("Visibility reuse cutoff: %u frames", limit);
            break;
        case ReservoirSubfieldVizMode::GIReservoir:
            limit = ui.restirGI.temporalResamplingParams.maxReservoirAge;
            ImGui::Text("Temporal age limit: %u frames", limit);
            break;
        case ReservoirSubfieldVizMode::PTReservoir:
            limit = ui.restirPT.temporalResampling.maxReservoirAge;
            ImGui::TextWrapped(ptAgeRejection
                ? "Age: sample survival."
                : "Age: temporal duplication tracking.");
            ImGui::Text("Rejection cutoff: %u frames (%s)", limit,
                ptAgeRejection ? "enabled" : "disabled");
            break;
        }
        if (ImGui::Button("Fit range to age cutoff"))
        {
            parameters.scalarMin = 0.f;
            parameters.scalarMax = float(std::max(limit, 1u));
            parameters.scalarVizFunction = RESERVOIR_VIZ_SCALAR_LINEAR;
        }
    }
}

ReservoirVisualizationSettings CreateReservoirVisualizationSettings()
{
    ReservoirVisualizationSettings settings = {};
    for (size_t index = 0; index < c_Fields.size(); ++index)
    {
        settings.fields[index] = MakeParameters(c_Fields[index]);
    }
    return settings;
}

uint32_t GetReservoirVisualizationFieldIndex(const UIDebugOutputSettings& settings)
{
    return uint32_t(GetFieldIndex(settings));
}

bool ReservoirVisualizationParametersMatch(const ReservoirVisualizationParameters& left, const ReservoirVisualizationParameters& right)
{
    // Display mode does not affect classification. Preserve the exact
    // remaining float bits so even invalid input settings can receive matching counts.
    ReservoirVisualizationParameters normalizedLeft = left;
    ReservoirVisualizationParameters normalizedRight = right;
    normalizedLeft.displayMode = 0;
    normalizedLeft.pad0 = 0;

    normalizedRight.displayMode = 0;
    normalizedRight.pad0 = 0;
    return std::memcmp(&normalizedLeft, &normalizedRight, sizeof(normalizedLeft)) == 0;
}

const ReservoirVisualizationParameters& GetReservoirVisualizationParameters(const UIDebugOutputSettings& settings)
{
    return settings.reservoirVisualization.fields[GetFieldIndex(settings)];
}

void DrawReservoirVisualizationControls(UIData& ui)
{
    auto& settings = ui.debugOutputSettings;
    const size_t index = GetFieldIndex(settings);
    const auto& field = c_Fields[index];
    auto& parameters = settings.reservoirVisualization.fields[index];
    ImGui::PushID("ReservoirVisualization");
    ImGui::TextUnformatted(field.label);
    if (field.explanation)
    {
        ImGui::TextWrapped("%s", field.explanation);
    }
    DrawDisplayModeControls(parameters);
    if (ImGui::Button("Reset this field's display"))
    {
        parameters = MakeParameters(field);
        settings.reservoirVisualization.positionBoundsInitialized[index] = false;
    }
    switch (field.kind)
    {
    case FieldKind::Light:
    {
        EditVisualizationMode("Light ID display", parameters.lightIdVizMode, c_LightIdChoices);
        if (parameters.lightIdVizMode == RESERVOIR_VIZ_LIGHT_ID_MATCH_REFERENCE)
        {
            ImGui::InputScalar("Reference light index", ImGuiDataType_U32, &parameters.referenceLightIndex);
            ImGui::TextUnformatted("White = match; dark = other.");
        }
        break;
    }
    case FieldKind::UV:
        if (parameters.vectorVizMode > RESERVOIR_VIZ_UV_V_GRAYSCALE)
        {
            parameters.vectorVizMode = RESERVOIR_VIZ_UV_RG_COORDINATES;
        }
        EditVisualizationMode("UV display", parameters.vectorVizMode, c_UVChoices);
        ImGui::TextUnformatted("U, V: [0, 1].");
        break;
    case FieldKind::Visibility:
        EditChoice("Visibility display", parameters.colorMode, "RGB transmittance\0Luminance\0Reuse eligibility\0");
        ImGui::Text("Reuse limits: age <= %u frames, distance < %.3g pixels",
            ui.restirDI.shadingParams.finalVisibilityMaxAge, ui.restirDI.shadingParams.finalVisibilityMaxDistance);
        if (parameters.colorMode == 2)
        {
            ImGui::TextWrapped("Gray = disabled; blue = age 0; orange = too old; purple = too far; green = reusable.");
        }
        else
        {
            ImGui::TextUnformatted("Black = blocked; white = clear.");
        }
        break;
    case FieldKind::Displacement:
        EditVisualizationMode("Displacement display", parameters.vectorVizMode, c_DisplacementChoices);
        ImGui::Text("Reuse cutoff: < %.3g pixels", ui.restirDI.shadingParams.finalVisibilityMaxDistance);
        if (parameters.vectorVizMode == RESERVOIR_VIZ_DISPLACEMENT_MAGNITUDE)
        {
            DrawScalarControls(parameters);
        }
        else
        {
            ImGui::InputFloat("Direction saturation distance", &parameters.displacementSaturationDistance, 0.f, 0.f, "%.4g");
            parameters.displacementSaturationDistance = std::isfinite(parameters.displacementSaturationDistance) ? std::max(parameters.displacementSaturationDistance, 0.001f) : 32.f;
            ImGui::TextWrapped("Hue = direction; saturation = distance.");
        }
        break;
    case FieldKind::Age:
        DrawAgeLimit(ui, parameters);
        DrawScalarControls(parameters, false);
        break;
    case FieldKind::Position:
        if (!settings.reservoirVisualization.positionBoundsInitialized[index])
        {
            FitPositionToScene(ui, parameters, .25f);
            settings.reservoirVisualization.positionBoundsInitialized[index] = true;
        }
        EditVisualizationMode("Position display", parameters.vectorVizMode, c_PositionChoices);
        ImGui::InputFloat3("World-space center", &parameters.positionCenter.x, "%.5g");
        if (settings.reservoirSubfieldVizMode == ReservoirSubfieldVizMode::PTReservoir
            && ImGui::Button("Set center to camera"))
        {
            parameters.positionCenter = ui.resources->renderedCameraPosition;
        }
        ImGui::InputFloat("Half extent (world units)", &parameters.positionExtent, 0.f, 0.f, "%.5g");
        parameters.positionExtent = std::isfinite(parameters.positionExtent) ? std::max(parameters.positionExtent, 0.001f) : 20.f;
        if (ImGui::Button("Fit position bounds to scene"))
        {
            FitPositionToScene(ui, parameters);
        }
        if (parameters.vectorVizMode == RESERVOIR_VIZ_POSITION_DISTANCE)
        {
            DrawScalarControls(parameters);
        }
        else
        {
            ImGui::TextUnformatted(parameters.vectorVizMode == RESERVOIR_VIZ_POSITION_XYZ
                ? "RGB = XYZ within the position bounds."
                : "Blue = negative; gray = zero; orange = positive.");
        }
        break;
    case FieldKind::Normal:
        EditVisualizationMode("Normal display", parameters.vectorVizMode, c_NormalChoices);
        ImGui::TextUnformatted(parameters.vectorVizMode == RESERVOIR_VIZ_NORMAL_RGB
            ? "RGB maps XYZ from [-1,1] to [0,1]."
            : "Blue = negative; gray = zero; orange = positive.");
        break;
    case FieldKind::PTRadiance:
        EditChoice("Display", parameters.colorMode,
            "Radiance (RGB)\0Luminance heatmap\0Light ID\0Light UV\0Payload type\0");
        switch (parameters.colorMode)
        {
        case RESERVOIR_VIZ_PT_RADIANCE_RGB:
            ImGui::SliderFloat("Brightness (stops)", &parameters.exposure, -20.f, 20.f, "%.1f");
            break;
        case RESERVOIR_VIZ_PT_RADIANCE_LUMINANCE:
            DrawScalarControls(parameters);
            break;
        case RESERVOIR_VIZ_PT_RADIANCE_LIGHT_UV:
            if (parameters.displayMode == RESERVOIR_VIZ_DISPLAY_VALUES)
            {
                ImGui::TextUnformatted("R = U; G = V.");
            }
            break;
        case RESERVOIR_VIZ_PT_RADIANCE_PAYLOAD_TYPE:
            if (parameters.displayMode == RESERVOIR_VIZ_DISPLAY_VALUES)
            {
                ImGui::TextUnformatted("Blue = radiance; orange = light data.");
            }
            break;
        }
        break;
    case FieldKind::Radiance:
    case FieldKind::Target:
        EditChoice("Color display", parameters.colorMode, "RGB\0Luminance heatmap\0");
        if (parameters.colorMode == 1)
        {
            DrawScalarControls(parameters);
        }
        else
        {
            ImGui::SliderFloat("Brightness (stops)", &parameters.exposure, -20.f, 20.f, "%.1f");
        }
        break;
    case FieldKind::Seed:
    case FieldKind::Index:
        DrawRngControls(parameters, field.kind == FieldKind::Index);
        break;
    case FieldKind::GIWeight:
    case FieldKind::PartialJacobian:
    case FieldKind::IntegerCount:
        DrawScalarControls(parameters, false);
        break;
    case FieldKind::Scalar:
    case FieldKind::Count:
        DrawScalarControls(parameters);
        break;
    case FieldKind::Unavailable:
        break;
    }
    if (parameters.displayMode == RESERVOIR_VIZ_DISPLAY_STATUS)
    {
        DrawStatusStatistics(parameters, settings.reservoirVisualization.statistics, uint32_t(index));
    }
    ImGui::PopID();
}
