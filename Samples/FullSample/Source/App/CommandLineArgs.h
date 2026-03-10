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

#pragma once

#include <nvrhi/nvrhi.h>

#include "App/SceneRenderer.h"

struct UIData;

namespace donut::app {
    struct DeviceCreationParameters;
}

struct CommandLineArguments
{
    nvrhi::GraphicsAPI graphicsApi = nvrhi::GraphicsAPI::VULKAN;
    bool disableBackgroundOptimization = false;
    SceneRendererStartupSettings appStartupSettings;
};

void ProcessCommandLine(int argc, char** argv, const std::string& appTitle, donut::app::DeviceCreationParameters& deviceParams, UIData& ui, CommandLineArguments& args);