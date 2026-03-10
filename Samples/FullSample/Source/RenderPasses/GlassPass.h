/*
 * SPDX-FileCopyrightText: Copyright (c) 2020-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "RayTracingPass.h"

#include <donut/core/math/math.h>
#include <nvrhi/nvrhi.h>
#include <memory>

namespace donut::engine
{
    class Scene;
    class CommonRenderPasses;
    class IView;
    class ShaderFactory;
}

class RenderTargets;
class Profiler;
class EnvironmentLight;
enum class IndirectLightingMode : uint32_t;

class GlassPass
{
public:

    GlassPass(
        nvrhi::IDevice* device,
        std::shared_ptr<donut::engine::ShaderFactory> shaderFactory,
        std::shared_ptr<donut::engine::CommonRenderPasses> commonPasses,
        std::shared_ptr<donut::engine::Scene> scene,
        std::shared_ptr<Profiler> profiler,
        nvrhi::IBindingLayout* bindlessLayout);

    void CreatePipeline(bool useRayQuery);

    void CreateBindingSet(
        nvrhi::rt::IAccelStruct* topLevelAS,
        nvrhi::rt::IAccelStruct* prevTopLevelAS,
        const RenderTargets& renderTargets);

    void Render(
        nvrhi::ICommandList* commandList,
        const donut::engine::IView& view,
        const EnvironmentLight& environmentLight,
        float normalMapScale,
        bool enableMaterialReadback,
        dm::int2 materialReadbackPosition,
        IndirectLightingMode indirectLightingMode);

    void NextFrame();

private:
    nvrhi::DeviceHandle m_device;

    RayTracingPass m_pass;
    nvrhi::BindingLayoutHandle m_bindingLayout;
    nvrhi::BindingLayoutHandle m_bindlessLayout;
    nvrhi::BindingSetHandle m_bindingSet;
    nvrhi::BindingSetHandle m_prevBindingSet;

    nvrhi::BufferHandle m_constantBuffer;

    std::shared_ptr<donut::engine::ShaderFactory> m_shaderFactory;
    std::shared_ptr<donut::engine::CommonRenderPasses> m_commonPasses;
    std::shared_ptr<donut::engine::Scene> m_scene;
    std::shared_ptr<Profiler> m_profiler;
};
