/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#include "VisualizationPass.h"

#include <donut/engine/CommonRenderPasses.h>
#include <donut/engine/ShaderFactory.h>
#include <donut/engine/View.h>
#include <nvrhi/utils.h>
#include <Rtxdi/ImportanceSamplingContext.h>

#include "../RenderTargets.h"
#include "../RtxdiResources.h"
#include "SharedShaderInclude/ShaderDebug/VisualizationOverlayMode.h"

using namespace donut::math;
#include "SharedShaderInclude/ShaderParameters.h"

using namespace donut::engine;

VisualizationPass::VisualizationPass(nvrhi::IDevice* device,
    CommonRenderPasses& commonPasses,
    ShaderFactory& shaderFactory,
    RenderTargets& renderTargets,
    RtxdiResources& rtxdiResources)
    : m_device(device),
    m_gpuPerfMarker("OverlayVisualizationPass")
{
    m_vertexShader = commonPasses.m_FullscreenVS;
    m_hdrPixelShader = shaderFactory.CreateShader("app/ShaderDebug/VisualizeHdrSignals.hlsl", "main", nullptr, nvrhi::ShaderType::Pixel);
    m_confidencePixelShader = shaderFactory.CreateShader("app/ShaderDebug/VisualizeConfidence.hlsl", "main", nullptr, nvrhi::ShaderType::Pixel);

    auto constantBufferDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(VisualizationConstants), "VisualizationConstants", 16);

    m_constantBuffer = device->createBuffer(constantBufferDesc);

    auto bindingDesc = nvrhi::BindingSetDesc()
        .addItem(nvrhi::BindingSetItem::Texture_SRV(0, renderTargets.HdrColor))
        .addItem(nvrhi::BindingSetItem::Texture_SRV(1, renderTargets.ResolvedColor))
        .addItem(nvrhi::BindingSetItem::Texture_SRV(2, renderTargets.AccumulatedColor))
        .addItem(nvrhi::BindingSetItem::Texture_SRV(3, renderTargets.DiffuseLighting))
        .addItem(nvrhi::BindingSetItem::Texture_SRV(4, renderTargets.SpecularLighting))
        .addItem(nvrhi::BindingSetItem::Texture_SRV(5, renderTargets.DenoisedDiffuseLighting))
        .addItem(nvrhi::BindingSetItem::Texture_SRV(6, renderTargets.DenoisedSpecularLighting))
        .addItem(nvrhi::BindingSetItem::Texture_SRV(7, renderTargets.Gradients))
        .addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(8, rtxdiResources.DIReservoirBuffer))
        .addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(9, rtxdiResources.GIReservoirBuffer))
        .addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_constantBuffer));

    nvrhi::utils::CreateBindingSetAndLayout(device, nvrhi::ShaderType::AllGraphics, 0, bindingDesc, m_hdrBindingLayout, m_hdrBindingSet);

    for (int currentFrame = 0; currentFrame <= 1; currentFrame++)
    {
        bindingDesc.bindings.resize(0);
        bindingDesc
            .addItem(nvrhi::BindingSetItem::Texture_SRV(0, currentFrame ? renderTargets.DiffuseConfidence : renderTargets.PrevDiffuseConfidence))
            .addItem(nvrhi::BindingSetItem::Texture_SRV(1, currentFrame ? renderTargets.SpecularConfidence : renderTargets.PrevSpecularConfidence))
            .addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_constantBuffer));

        nvrhi::BindingSetHandle bindingSet;
        nvrhi::utils::CreateBindingSetAndLayout(device, nvrhi::ShaderType::AllGraphics, 0, bindingDesc, m_confidenceBindingLayout, bindingSet);

        if (currentFrame)
            m_confidenceBindingSet = bindingSet;
        else
            m_confidenceBindingSetPrev = bindingSet;
    }
}

void VisualizationPass::Render(
    nvrhi::ICommandList* commandList,
    nvrhi::IFramebuffer* framebuffer,
    const IView& renderView,
    const IView& upscaledView,
    const rtxdi::ImportanceSamplingContext& isContext,
    uint32_t inputBufferIndex,
    VisualizationOverlayMode visualizationMode,
    bool enableAccumulation)
{
    commandList->beginMarker(m_gpuPerfMarker.c_str());
    if (m_hdrPipeline == nullptr || m_hdrPipeline->getFramebufferInfo() != framebuffer->getFramebufferInfo())
    {
        auto pipelineDesc = nvrhi::GraphicsPipelineDesc()
            .setVertexShader(m_vertexShader)
            .setPixelShader(m_hdrPixelShader)
            .addBindingLayout(m_hdrBindingLayout)
            .setPrimType(nvrhi::PrimitiveType::TriangleStrip)
            .setRenderState(nvrhi::RenderState()
                .setDepthStencilState(nvrhi::DepthStencilState().disableDepthTest().disableStencil())
                .setRasterState(nvrhi::RasterState().setCullNone())
                .setBlendState(nvrhi::BlendState().setRenderTarget(0, 
                    nvrhi::utils::CreateAddBlendState(nvrhi::BlendFactor::One, nvrhi::BlendFactor::InvSrcAlpha))));

        m_hdrPipeline = m_device->createGraphicsPipeline(pipelineDesc, framebuffer);

        pipelineDesc.setPixelShader(m_confidencePixelShader);
        pipelineDesc.bindingLayouts.resize(0);
        pipelineDesc.addBindingLayout(m_confidenceBindingLayout);

        m_confidencePipeline = m_device->createGraphicsPipeline(pipelineDesc, framebuffer);
    }

    bool confidence = 
        (visualizationMode == VisualizationOverlayMode::VISUALIZATION_OVERLAY_MODE_DIFFUSE_CONFIDENCE) ||
        (visualizationMode == VisualizationOverlayMode::VISUALIZATION_OVERLAY_MODE_SPECULAR_CONFIDENCE);

    auto state = nvrhi::GraphicsState()
        .setPipeline(confidence ? m_confidencePipeline : m_hdrPipeline)
        .addBindingSet(confidence ? m_confidenceBindingSet : m_hdrBindingSet)
        .setFramebuffer(framebuffer)
        .setViewport(upscaledView.GetViewportState());

    VisualizationConstants constants = {};
    constants.outputSize.x = upscaledView.GetViewExtent().width();
    constants.outputSize.y = upscaledView.GetViewExtent().height();
    const auto& renderViewport = renderView.GetViewportState().viewports[0];
    const auto& upscaledViewport = upscaledView.GetViewportState().viewports[0];
    constants.resolutionScale.x = renderViewport.width() / upscaledViewport.width();
    constants.resolutionScale.y = renderViewport.height() / upscaledViewport.height();
    constants.restirDIReservoirBufferParams = isContext.GetReSTIRDIContext().GetReservoirBufferParameters();
    constants.restirGIReservoirBufferParams = isContext.GetReSTIRGIContext().GetReservoirBufferParameters();
    constants.visualizationMode = (uint32_t)visualizationMode;
    constants.inputBufferIndex = inputBufferIndex;
    constants.enableAccumulation = enableAccumulation;
    commandList->writeBuffer(m_constantBuffer, &constants, sizeof(constants));

    commandList->setGraphicsState(state);
    commandList->draw(nvrhi::DrawArguments().setVertexCount(4));
    commandList->endMarker();
}

void VisualizationPass::NextFrame()
{
    std::swap(m_confidenceBindingSet, m_confidenceBindingSetPrev);
}
