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

#include "ConfidencePass.h"
#include "FilterGradientsPass.h"
#include "RenderTargets.h"

#include <donut/engine/ShaderFactory.h>
#include <donut/engine/View.h>
#include <donut/core/log.h>
#include <nvrhi/utils.h>


using namespace donut::math;
#include "SharedShaderInclude/ShaderParameters.h"

using namespace donut::engine;

ConfidencePass::ConfidencePass(
    nvrhi::IDevice* device, 
    std::shared_ptr<ShaderFactory> shaderFactory)
    : m_device(device)
    , m_shaderFactory(shaderFactory)
{
    nvrhi::BindingLayoutDesc bindingLayoutDesc;
    bindingLayoutDesc.visibility = nvrhi::ShaderType::Compute;
    bindingLayoutDesc.bindings = {
        nvrhi::BindingLayoutItem::Texture_SRV(0),
        nvrhi::BindingLayoutItem::Texture_SRV(1),
        nvrhi::BindingLayoutItem::Texture_SRV(2),
        nvrhi::BindingLayoutItem::Texture_SRV(3),
        nvrhi::BindingLayoutItem::Texture_UAV(0),
        nvrhi::BindingLayoutItem::Texture_UAV(1),
        nvrhi::BindingLayoutItem::Sampler(0),
        nvrhi::BindingLayoutItem::PushConstants(0, sizeof(ConfidenceConstants))
    };

    m_bindingLayout = m_device->createBindingLayout(bindingLayoutDesc);

    auto samplerDesc = nvrhi::SamplerDesc()
        .setAllFilters(true)
        .setAllAddressModes(nvrhi::SamplerAddressMode::ClampToBorder)
        .setBorderColor(nvrhi::Color(0.f));

    m_sampler = device->createSampler(samplerDesc);
}

void ConfidencePass::CreatePipeline()
{
    donut::log::debug("Initializing ConfidencePass...");

    m_computeShader = m_shaderFactory->CreateShader("app/DenoisingPasses/ConfidencePass.hlsl", "main", nullptr, nvrhi::ShaderType::Compute);

    nvrhi::ComputePipelineDesc pipelineDesc;
    pipelineDesc.bindingLayouts = { m_bindingLayout };
    pipelineDesc.CS = m_computeShader;
    m_computePipeline = m_device->createComputePipeline(pipelineDesc);
}

void ConfidencePass::CreateBindingSet(const RenderTargets& renderTargets)
{
    for (int currentFrame = 0; currentFrame <= 1; currentFrame++)
    {
        nvrhi::BindingSetDesc bindingSetDesc;

        bindingSetDesc.bindings = {
            nvrhi::BindingSetItem::Texture_SRV(0, renderTargets.Gradients),
            nvrhi::BindingSetItem::Texture_SRV(1, renderTargets.MotionVectors),
            nvrhi::BindingSetItem::Texture_SRV(2, currentFrame ? renderTargets.PrevDiffuseConfidence : renderTargets.DiffuseConfidence),
            nvrhi::BindingSetItem::Texture_SRV(3, currentFrame ? renderTargets.PrevSpecularConfidence : renderTargets.SpecularConfidence),
            nvrhi::BindingSetItem::Texture_UAV(0, currentFrame ? renderTargets.DiffuseConfidence : renderTargets.PrevDiffuseConfidence),
            nvrhi::BindingSetItem::Texture_UAV(1, currentFrame ? renderTargets.SpecularConfidence : renderTargets.PrevSpecularConfidence),
            nvrhi::BindingSetItem::Sampler(0, m_sampler),
            nvrhi::BindingSetItem::PushConstants(0, sizeof(ConfidenceConstants))
        };

        nvrhi::BindingSetHandle bindingSet = m_device->createBindingSet(bindingSetDesc, m_bindingLayout);
        if (currentFrame)
            m_bindingSet = bindingSet;
        else
            m_prevBindingSet = bindingSet;
    }

    m_gradientsTexture = renderTargets.Gradients;
}

void ConfidencePass::Render(
    nvrhi::ICommandList* commandList, 
    const donut::engine::IView& view,
    float logDarknessBias,
    float sensitivity,
    float historyLength,
    bool checkerboard)
{
    commandList->beginMarker("Confidence");

    const auto& gradientsDesc = m_gradientsTexture->getDesc();

    ConfidenceConstants constants = {};
    constants.viewportSize = dm::uint2(view.GetViewExtent().width(), view.GetViewExtent().height());
    constants.invGradientTextureSize.x = 1.f / float(gradientsDesc.width);
    constants.invGradientTextureSize.y = 1.f / float(gradientsDesc.height);
    constants.darknessBias = ::exp2f(logDarknessBias);
    constants.sensitivity = sensitivity;
    constants.checkerboard = checkerboard;
    constants.blendFactor = 1.f / (historyLength + 1.f);
    constants.inputBufferIndex = FilterGradientsPass::GetOutputBufferIndex();

    nvrhi::ComputeState state;
    state.bindings = { m_bindingSet };
    state.pipeline = m_computePipeline;
    commandList->setComputeState(state);

    commandList->setPushConstants(&constants, sizeof(constants));
    
    commandList->dispatch(
        dm::div_ceil(view.GetViewExtent().width(), 8), 
        dm::div_ceil(view.GetViewExtent().height(), 8), 
        1);

    commandList->endMarker();
}

void ConfidencePass::NextFrame()
{
    std::swap(m_bindingSet, m_prevBindingSet);
}
