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

#include "PTReservoirVizPass.h"

#include <donut/core/math/math.h>
using namespace donut;
using namespace donut::math;

#include "../../RenderTargets.h"
#include "../../Scene/SampleScene.h"
#include "../../UserInterface.h"

#include <donut/engine/ShaderFactory.h>
#include <donut/engine/View.h>
#include <donut/engine/Scene.h>
#include <donut/engine/CommonRenderPasses.h>
#include <donut/core/log.h>
#include <nvrhi/utils.h>

#include "SharedShaderInclude/ShaderDebug/ReservoirSubfieldVizPasses/PTReservoirVizParameters.h"

#include <utility>

using namespace donut::math;
using namespace donut::engine;

PTReservoirVizPass::PTReservoirVizPass(
    nvrhi::IDevice* device,
    std::shared_ptr<donut::engine::ShaderFactory> shaderFactory,
    std::shared_ptr<donut::engine::Scene> scene,
    nvrhi::IBindingLayout* bindlessLayout) :
    m_device(device),
    m_bindlessLayout(bindlessLayout),
    m_shaderFactory(std::move(shaderFactory)),
    m_scene(std::move(scene)),
    m_gpuPerfMarker("PTReservoirVizPass")
{
    nvrhi::BufferDesc cbDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(PTReservoirVizParameters), "PTReservoirVizParameters", 16);;
    m_constantBuffer = m_device->createBuffer(cbDesc);

    CreateBindingLayout();
    CreatePipeline();
}

void PTReservoirVizPass::CreateBindingLayout()
{
    nvrhi::BindingLayoutDesc bindingLayoutDesc;
    bindingLayoutDesc.visibility = nvrhi::ShaderType::Compute;
    bindingLayoutDesc.bindings = {
        nvrhi::BindingLayoutItem::StructuredBuffer_UAV(0),
        nvrhi::BindingLayoutItem::Texture_UAV(1),
        nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
        nvrhi::BindingLayoutItem::VolatileConstantBuffer(1)
    };

    m_bindingLayout = m_device->createBindingLayout(bindingLayoutDesc);
}

void PTReservoirVizPass::CreatePipeline()
{
    std::string shaderPath = "app/ShaderDebug/ReservoirSubfieldVizPasses/PTReservoirViz.hlsl";
    m_computeShader = m_shaderFactory->CreateShader(shaderPath.c_str(), "main", nullptr, nvrhi::ShaderType::Compute);

    nvrhi::ComputePipelineDesc pipelineDesc;
    pipelineDesc.bindingLayouts = { m_bindingLayout, m_bindlessLayout };
    pipelineDesc.CS = m_computeShader;
    m_computePipeline = m_device->createComputePipeline(pipelineDesc);
}

void PTReservoirVizPass::CreateBindingSet(nvrhi::BufferHandle src, nvrhi::TextureHandle dst, nvrhi::BufferHandle cbuf)
{
    nvrhi::BindingSetDesc bindingSetDesc;

    bindingSetDesc.bindings = {
        nvrhi::BindingSetItem::StructuredBuffer_UAV(0, src),
        nvrhi::BindingSetItem::Texture_UAV(1, dst),
        nvrhi::BindingSetItem::ConstantBuffer(0, m_constantBuffer),
        nvrhi::BindingSetItem::ConstantBuffer(1, cbuf)
    };

    m_bindingSet = m_device->createBindingSet(bindingSetDesc, m_bindingLayout);
}

void PTReservoirVizPass::Render(
    nvrhi::ICommandList* commandList,
    const donut::engine::IView& view,
    RTXDI_RuntimeParameters runtimeParams,
    RTXDI_ReservoirBufferParameters reservoirBufferParams,
    RTXDI_PTBufferIndices bufferIndices,
    PTReservoirField ptReservoirField)
{
    commandList->beginMarker(m_gpuPerfMarker.c_str());

    PTReservoirVizParameters cbData;
    view.FillPlanarViewConstants(cbData.view);
    cbData.runtimeParams = runtimeParams;
    cbData.reservoirBufferParams = reservoirBufferParams;
    cbData.bufferIndices = bufferIndices;
    cbData.ptReservoirField = ptReservoirField;

    commandList->writeBuffer(m_constantBuffer, &cbData, sizeof(cbData));

    nvrhi::ComputeState state;
    state.bindings = { m_bindingSet, m_scene->GetDescriptorTable() };
    state.pipeline = m_computePipeline;
    commandList->setComputeState(state);

    commandList->dispatch(
        dm::div_ceil(view.GetViewExtent().width(), 16),
        dm::div_ceil(view.GetViewExtent().height(), 16),
        1);

    commandList->endMarker();
}