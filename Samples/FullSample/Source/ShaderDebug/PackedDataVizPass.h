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

#include <nvrhi/nvrhi.h>
#include <memory>

namespace donut::engine
{
    class Scene;
    class CommonRenderPasses;
    class ShaderFactory;
    class IView;
}

class RenderTargets;
class EnvironmentLight;
struct UIData;

class PackedDataVizPass
{
public:
    PackedDataVizPass(
        nvrhi::IDevice* device,
        std::shared_ptr<donut::engine::ShaderFactory> shaderFactory,
        std::shared_ptr<donut::engine::Scene> scene,
        nvrhi::IBindingLayout* bindlessLayout);

    void CreatePipeline(const std::string& shaderPath);

    void CreateBindingSet(nvrhi::TextureHandle src, nvrhi::TextureHandle prevSrc, nvrhi::TextureHandle dst);

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
    nvrhi::BindingSetHandle m_bindingSetEven;
    nvrhi::BindingSetHandle m_bindingSetOdd;

    nvrhi::BufferHandle m_constantBuffer;

    std::shared_ptr<donut::engine::ShaderFactory> m_shaderFactory;
    std::shared_ptr<donut::engine::Scene> m_scene;
    std::string m_gpuPerfMarker;
};
