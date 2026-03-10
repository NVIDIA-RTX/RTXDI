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

#include "PTPathVizLinesPass.h"

#include <donut/core/log.h>
#include <donut/core/math/math.h>
#include <donut/engine/FramebufferFactory.h>
#include <donut/engine/ShaderFactory.h>
#include <donut/engine/View.h>
#include <nvrhi/utils.h>

#include "../../RenderTargets.h"

using namespace donut::math;
#include "SharedShaderInclude/ShaderDebug/PTPathViz/PTPathVizConstants.h"

PTPathVizLinesPass::PTPathVizLinesPass(
    nvrhi::IDevice* device,
    std::shared_ptr<donut::engine::ShaderFactory> shaderFactory,
    std::shared_ptr<donut::engine::Scene> scene)
    : m_device(device)
    , m_shaderFactory(std::move(shaderFactory))
    , m_scene(std::move(scene))
    , m_isCameraToPrimarySurfaceRayEnabled(true)
    , m_isDepthTestEnabled(true)
{
    m_constantBuffer = m_device->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(PTPathVizLineConstants), "PTPathVizLineConstants", 16));

    CreateBindingLayout();
}

void PTPathVizLinesPass::CreateBindingSet()
{
    nvrhi::BindingSetDesc bindingSetDesc;

    bindingSetDesc.bindings = {
        nvrhi::BindingSetItem::ConstantBuffer(0, m_constantBuffer)
    };

    m_bindingSet = m_device->createBindingSet(bindingSetDesc, m_bindingLayout);
}

static std::vector<nvrhi::VertexAttributeDesc> GetVertexAttributeDescs()
{
    std::vector<nvrhi::VertexAttributeDesc> descs;

    nvrhi::VertexAttributeDesc positionDesc;
    positionDesc.setName("POSITION");
    positionDesc.setFormat(nvrhi::Format::RGBA32_FLOAT);
    positionDesc.setBufferIndex(0);
    positionDesc.setOffset(0);
    positionDesc.setElementStride(sizeof(float4));
    positionDesc.setIsInstanced(false);

    descs.push_back(positionDesc);

    return descs;
}

void PTPathVizLinesPass::CreatePipeline(const RenderTargets& renderTargets)
{
    donut::log::debug("Initializing PTPathVizLinesPass...");

    using namespace donut::engine;
    using namespace nvrhi;

    m_framebufferWithDepth = std::make_shared<FramebufferFactory>(m_device);
    m_framebufferWithDepth->DepthTarget = renderTargets.DeviceDepth;
    m_framebufferWithDepth->RenderTargets = {
        renderTargets.LdrColor
    };

    m_framebufferWithoutDepth = std::make_shared<FramebufferFactory>(m_device);
    m_framebufferWithoutDepth->RenderTargets = {
        renderTargets.LdrColor
    };

    nvrhi::GraphicsPipelineDesc pipelineDesc;

    pipelineDesc.bindingLayouts = { m_bindingLayout };
    pipelineDesc.VS = m_shaderFactory->CreateShader("app/ShaderDebug/PTPathViz/PTPathVizLines_vs.hlsl", "vs_main", nullptr, nvrhi::ShaderType::Vertex);
    pipelineDesc.PS = m_shaderFactory->CreateShader("app/ShaderDebug/PTPathViz/PTPathVizLines_ps.hlsl", "ps_main", nullptr, nvrhi::ShaderType::Pixel);
    pipelineDesc.primType = nvrhi::PrimitiveType::LineList;
    std::vector<VertexAttributeDesc> vaDescs = GetVertexAttributeDescs();

    pipelineDesc.inputLayout = m_device->createInputLayout(vaDescs.data(), vaDescs.size(), pipelineDesc.VS);
    pipelineDesc.renderState.blendState.alphaToCoverageEnable = false;
    pipelineDesc.renderState.depthStencilState.depthTestEnable = true;
    pipelineDesc.renderState.depthStencilState.depthFunc = nvrhi::ComparisonFunc::Greater;
    pipelineDesc.renderState.rasterState.frontCounterClockwise = true;
    pipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;

    auto* framebuffer = m_framebufferWithDepth->GetFramebuffer(nvrhi::AllSubresources);
    m_pipelineWithDepth = m_device->createGraphicsPipeline(pipelineDesc, framebuffer);

    pipelineDesc.renderState.depthStencilState.depthTestEnable = false;
    framebuffer = m_framebufferWithoutDepth->GetFramebuffer(nvrhi::AllSubresources);
    m_pipelineWithoutDepth = m_device->createGraphicsPipeline(pipelineDesc, framebuffer);
}

void PTPathVizLinesPass::CreateBindingLayout()
{
    nvrhi::BindingLayoutDesc globalBindingLayoutDesc;
    globalBindingLayoutDesc.visibility = nvrhi::ShaderType::AllGraphics;
    globalBindingLayoutDesc.bindings = {
        nvrhi::BindingLayoutItem::VolatileConstantBuffer(0)
    };

    m_bindingLayout = m_device->createBindingLayout(globalBindingLayoutDesc);
}

void PTPathVizLinesPass::EnableDepthTest(bool enable)
{
    m_isDepthTestEnabled = enable;
}

bool PTPathVizLinesPass::IsDepthTestEnabled() const
{
    return m_isDepthTestEnabled;
}

void PTPathVizLinesPass::RenderLines(
    nvrhi::ICommandList* commandList,
    const donut::engine::IView& view,
    nvrhi::BufferHandle lineListBuffer,
    const PTPathVizPaths& paths,
    uint32_t cameraVertexEnabled,
    uint32_t numLinesPerPath,
    uint32_t numPaths)
{
    commandList->beginMarker("PTPathVizLinesPass");

    commandList->setEnableAutomaticBarriers(false);
    commandList->setResourceStatesForFramebuffer(m_framebufferWithDepth->GetFramebuffer(nvrhi::AllSubresources));
    commandList->commitBarriers();

    uint32_t totalLines = numLinesPerPath * numPaths;
    UpdateConstantBuffer(commandList, view, paths, cameraVertexEnabled, numLinesPerPath);
    SetGraphicsState(commandList, view, lineListBuffer);
    DrawLines(commandList, totalLines);

    commandList->setEnableAutomaticBarriers(true);
    commandList->endMarker();
}

void PTPathVizLinesPass::UpdateConstantBuffer(nvrhi::ICommandList* commandList,
                                              const donut::engine::IView& view,
                                              const PTPathVizPaths& paths,
                                              uint32_t cameraVertexEnabled,
                                              uint32_t maxLines)
{
    PTPathVizLineConstants cbData = {};
    view.FillPlanarViewConstants(cbData.view);
    cbData.paths = paths;
    cbData.enableCameraVertex = cameraVertexEnabled;
    cbData.maxVerticesPerPath = 2 * maxLines;
    commandList->writeBuffer(m_constantBuffer, &cbData, sizeof(cbData));
}

void PTPathVizLinesPass::SetGraphicsState(nvrhi::ICommandList* commandList,
    const donut::engine::IView& view,
    nvrhi::BufferHandle lineListBuffer)
{
    nvrhi::GraphicsState state;
    state.pipeline = m_isDepthTestEnabled ? m_pipelineWithDepth : m_pipelineWithoutDepth;
    state.bindings = { m_bindingSet };
    state.framebuffer = (m_isDepthTestEnabled ? m_framebufferWithDepth : m_framebufferWithoutDepth)->GetFramebuffer(nvrhi::AllSubresources);
    nvrhi::VertexBufferBinding vbb = {};
    vbb.buffer = lineListBuffer;
    vbb.offset = 0;
    vbb.slot = 0;
    state.addVertexBuffer(vbb);
    state.viewport = view.GetViewportState();
    commandList->setGraphicsState(state);
}

void PTPathVizLinesPass::DrawLines(nvrhi::ICommandList* commandList, uint numLines)
{
    nvrhi::DrawArguments args{};
    args.vertexCount = numLines * 2; // Each path has two vertices (start and end)
    commandList->draw(args);
}
