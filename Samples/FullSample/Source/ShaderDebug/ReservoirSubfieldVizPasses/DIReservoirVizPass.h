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

struct RTXDI_RuntimeParameters;
struct RTXDI_ReservoirBufferParameters;
struct RTXDI_DIBufferIndices;
enum class DIReservoirField;

class RenderTargets;
class EnvironmentLight;
struct UIData;

class DIReservoirVizPass
{
public:
    DIReservoirVizPass(
        nvrhi::IDevice* device,
        std::shared_ptr<donut::engine::ShaderFactory> shaderFactory,
        std::shared_ptr<donut::engine::Scene> scene,
        nvrhi::IBindingLayout* bindlessLayout);

    void CreateBindingSet(nvrhi::BufferHandle src, nvrhi::TextureHandle dst, nvrhi::BufferHandle cbuf);

    void Render(
        nvrhi::ICommandList* commandList,
        const donut::engine::IView& view,
        RTXDI_RuntimeParameters runtimeParams,
        RTXDI_ReservoirBufferParameters reservoirBufferParams,
        RTXDI_DIBufferIndices bufferIndices,
        DIReservoirField diReservoirField,
        uint32_t maxLightsInBuffer);

private:
    void CreateBindingLayout();
    void CreatePipeline();

    nvrhi::DeviceHandle m_device;

    nvrhi::ShaderHandle m_computeShader;
    nvrhi::ComputePipelineHandle m_computePipeline;
    nvrhi::BindingLayoutHandle m_bindingLayout;
    nvrhi::BindingLayoutHandle m_bindlessLayout;
    nvrhi::BindingSetHandle m_bindingSet;

    nvrhi::BufferHandle m_constantBuffer;

    std::shared_ptr<donut::engine::ShaderFactory> m_shaderFactory;
    std::shared_ptr<donut::engine::Scene> m_scene;
    std::string m_gpuPerfMarker;
};
