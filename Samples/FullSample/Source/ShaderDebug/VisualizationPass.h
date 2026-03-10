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

#include "../RenderPasses/RayTracingPass.h"

#include <nvrhi/nvrhi.h>
#include <memory>

namespace rtxdi
{
    class ImportanceSamplingContext;
}

namespace donut::engine
{
    class IView;
    class ShaderFactory;
    class CommonRenderPasses;
}

enum class VisualizationOverlayMode;

class RenderTargets;
class RtxdiResources;

class VisualizationPass
{
public:
    VisualizationPass(
        nvrhi::IDevice* device,
        donut::engine::CommonRenderPasses& commonPasses,
        donut::engine::ShaderFactory& shaderFactory,
        RenderTargets& renderTargets,
        RtxdiResources& rtxdiResources);

    void Render(
        nvrhi::ICommandList* commandList,
        nvrhi::IFramebuffer* framebuffer,
        const donut::engine::IView& renderView,
        const donut::engine::IView& upscaledView,
        const rtxdi::ImportanceSamplingContext& context,
        uint32_t inputBufferIndex,
        VisualizationOverlayMode visualizationMode,
        bool enableAccumulation);

    void NextFrame();

private:
    nvrhi::DeviceHandle m_device;
    std::string m_gpuPerfMarker;

    nvrhi::BindingLayoutHandle m_hdrBindingLayout;
    nvrhi::BindingLayoutHandle m_confidenceBindingLayout;
    nvrhi::BindingSetHandle m_hdrBindingSet;
    nvrhi::BindingSetHandle m_confidenceBindingSet;
    nvrhi::BindingSetHandle m_confidenceBindingSetPrev;
    nvrhi::ShaderHandle m_vertexShader;
    nvrhi::ShaderHandle m_hdrPixelShader;
    nvrhi::ShaderHandle m_confidencePixelShader;
    nvrhi::GraphicsPipelineHandle m_hdrPipeline;
    nvrhi::GraphicsPipelineHandle m_confidencePipeline;

    nvrhi::BufferHandle m_constantBuffer;
};
