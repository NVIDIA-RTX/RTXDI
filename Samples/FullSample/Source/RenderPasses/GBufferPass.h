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
class SampleScene;

typedef int ibool;

struct GBufferSettings
{
    float roughnessOverride = 0.5f;
    float metalnessOverride = 0.5f;
    bool enableRoughnessOverride = false;
    bool enableMetalnessOverride = false;
    float normalMapScale = 1.f;
    ibool enableAlphaTestedGeometry = true;
    ibool enableTransparentGeometry = true;
    float textureLodBias = -1.f;

    bool enableMaterialReadback = false;
    dm::int2 materialReadbackPosition = 0;
};

class RaytracedGBufferPass
{
public:
    RaytracedGBufferPass(
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
        const donut::engine::IView& viewPrev,
        const GBufferSettings& settings);

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

class RasterizedGBufferPass
{
public:
    RasterizedGBufferPass(
        nvrhi::IDevice* device,
        std::shared_ptr<donut::engine::ShaderFactory> shaderFactory,
        std::shared_ptr<donut::engine::CommonRenderPasses> commonPasses,
        std::shared_ptr<donut::engine::Scene> scene,
        std::shared_ptr<Profiler> profiler,
        nvrhi::IBindingLayout* bindlessLayout);

    void CreatePipeline(const RenderTargets& renderTargets);

    void CreateBindingSet();

    void Render(
        nvrhi::ICommandList* commandList,
        const donut::engine::IView& view,
        const donut::engine::IView& viewPrev,
        const RenderTargets& renderTargets,
        const GBufferSettings& settings);

private:
    nvrhi::DeviceHandle m_device;

    nvrhi::GraphicsPipelineHandle m_opaquePipeline;
    nvrhi::GraphicsPipelineHandle m_alphaTestedPipeline;
    nvrhi::BindingLayoutHandle m_bindingLayout;
    nvrhi::BindingLayoutHandle m_bindlessLayout;
    nvrhi::BindingSetHandle m_bindingSet;

    nvrhi::BufferHandle m_constantBuffer;

    std::shared_ptr<donut::engine::ShaderFactory> m_shaderFactory;
    std::shared_ptr<donut::engine::CommonRenderPasses> m_commonPasses;
    std::shared_ptr<donut::engine::Scene> m_scene;
    std::shared_ptr<Profiler> m_profiler;
};

class PostprocessGBufferPass
{
public:
    PostprocessGBufferPass(
        nvrhi::IDevice* device,
        std::shared_ptr<donut::engine::ShaderFactory> shaderFactory);

    void CreatePipeline();

    void CreateBindingSet(
        const RenderTargets& renderTargets);

    void Render(
        nvrhi::ICommandList* commandList,
        const donut::engine::IView& view);

    void NextFrame();

private:
    nvrhi::DeviceHandle m_device;

    nvrhi::ShaderHandle m_computeShader;
    nvrhi::ComputePipelineHandle m_computePipeline;
    nvrhi::BindingLayoutHandle m_bindingLayout;
    nvrhi::BindingLayoutHandle m_bindlessLayout;
    nvrhi::BindingSetHandle m_bindingSet;
    nvrhi::BindingSetHandle m_prevBindingSet;

    std::shared_ptr<donut::engine::ShaderFactory> m_shaderFactory;
};