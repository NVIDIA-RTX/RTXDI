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

#include "PackedDataVizPass.h"

#include <donut/engine/ShaderFactory.h>
#include <donut/engine/View.h>
#include <donut/engine/Scene.h>
#include <donut/core/log.h>

#include <utility>

using namespace donut::math;
using namespace donut::engine;

PackedDataVizPass::PackedDataVizPass(
    nvrhi::IDevice* device,
    std::shared_ptr<donut::engine::ShaderFactory> shaderFactory,
    std::shared_ptr<donut::engine::Scene> scene,
    nvrhi::IBindingLayout* bindlessLayout) :
    m_device(device)
    , m_bindlessLayout(bindlessLayout)
    , m_shaderFactory(std::move(shaderFactory))
    , m_scene(std::move(scene))
{
    nvrhi::BindingLayoutDesc bindingLayoutDesc;
    bindingLayoutDesc.visibility = nvrhi::ShaderType::Compute;
    bindingLayoutDesc.bindings = {
        nvrhi::BindingLayoutItem::Texture_SRV(0),
        nvrhi::BindingLayoutItem::Texture_UAV(0),
    };

    m_bindingLayout = m_device->createBindingLayout(bindingLayoutDesc);
}

void PackedDataVizPass::CreatePipeline(const std::string& shaderPath)
{
    std::string debugMsg = "Initializing PackedDataVizPass with " + shaderPath + "...";
    donut::log::debug(debugMsg.c_str());
    m_gpuPerfMarker = "Packed Data Viz Pass:" + shaderPath;

    m_computeShader = m_shaderFactory->CreateShader(shaderPath.c_str(), "main", nullptr, nvrhi::ShaderType::Compute);

    nvrhi::ComputePipelineDesc pipelineDesc;
    pipelineDesc.bindingLayouts = { m_bindingLayout, m_bindlessLayout };
    pipelineDesc.CS = m_computeShader;
    m_computePipeline = m_device->createComputePipeline(pipelineDesc);
}

void PackedDataVizPass::CreateBindingSet(nvrhi::TextureHandle src, nvrhi::TextureHandle prevSrc, nvrhi::TextureHandle dst)
{
    nvrhi::BindingSetDesc bindingSetDesc;

    bindingSetDesc.bindings = {
        // GBuffer bindings --- CAUTION: these items are addressed below to swap the even and odd frames
        nvrhi::BindingSetItem::Texture_SRV(0, src),
        nvrhi::BindingSetItem::Texture_UAV(0, dst)
    };

    m_bindingSetEven = m_device->createBindingSet(bindingSetDesc, m_bindingLayout);

    bindingSetDesc.bindings[0].resourceHandle = prevSrc;

    m_bindingSetOdd = m_device->createBindingSet(bindingSetDesc, m_bindingLayout);
}

void PackedDataVizPass::Render(
    nvrhi::ICommandList* commandList,
    const donut::engine::IView& view)
{
    commandList->beginMarker(m_gpuPerfMarker.c_str());

    nvrhi::ComputeState state;
    state.bindings = { m_bindingSetEven, m_scene->GetDescriptorTable() };
    state.pipeline = m_computePipeline;
    commandList->setComputeState(state);

    commandList->dispatch(
        dm::div_ceil(view.GetViewExtent().width(), 16),
        dm::div_ceil(view.GetViewExtent().height(), 16),
        1);

    commandList->endMarker();
}

void PackedDataVizPass::NextFrame()
{
    std::swap(m_bindingSetEven, m_bindingSetOdd);
}