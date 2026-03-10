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

#include "FilterGradientsPass.h"
#include "RenderTargets.h"

#include <donut/engine/ShaderFactory.h>
#include <donut/engine/View.h>
#include <donut/core/log.h>
#include <nvrhi/utils.h>

using namespace donut::math;
#include "SharedShaderInclude/ShaderParameters.h"

using namespace donut::engine;

static const int c_NumFilterPasses = 4;

FilterGradientsPass::FilterGradientsPass(
    nvrhi::IDevice* device,
    std::shared_ptr<ShaderFactory> shaderFactory)
    : m_device(device)
    , m_shaderFactory(shaderFactory)
{
    nvrhi::BindingLayoutDesc bindingLayoutDesc;
    bindingLayoutDesc.visibility = nvrhi::ShaderType::Compute;
    bindingLayoutDesc.bindings = {
        nvrhi::BindingLayoutItem::Texture_UAV(0),
        nvrhi::BindingLayoutItem::PushConstants(0, sizeof(FilterGradientsConstants))
    };

    m_bindingLayout = m_device->createBindingLayout(bindingLayoutDesc);
}

void FilterGradientsPass::CreatePipeline()
{
    donut::log::debug("Initializing FilterGradientsPass...");

    m_computeShader = m_shaderFactory->CreateShader("app/DenoisingPasses/FilterGradientsPass.hlsl", "main", nullptr, nvrhi::ShaderType::Compute);

    nvrhi::ComputePipelineDesc pipelineDesc;
    pipelineDesc.bindingLayouts = { m_bindingLayout };
    pipelineDesc.CS = m_computeShader;
    m_computePipeline = m_device->createComputePipeline(pipelineDesc);
}

void FilterGradientsPass::CreateBindingSet(const RenderTargets& renderTargets)
{
    nvrhi::BindingSetDesc bindingSetDesc;

    bindingSetDesc.bindings = {
        nvrhi::BindingSetItem::Texture_UAV(0, renderTargets.Gradients),
        nvrhi::BindingSetItem::PushConstants(0, sizeof(FilterGradientsConstants))
    };

    m_bindingSet = m_device->createBindingSet(bindingSetDesc, m_bindingLayout);

    m_gradientsTexture = renderTargets.Gradients;
}

void FilterGradientsPass::Render(
    nvrhi::ICommandList* commandList,
    const donut::engine::IView& view,
    bool checkerboard)
{
    commandList->beginMarker("Filter Gradients");

    FilterGradientsConstants constants = {};
    constants.viewportSize = dm::uint2(view.GetViewExtent().width(), view.GetViewExtent().height());
    if (checkerboard) constants.viewportSize.x /= 2;
    constants.checkerboard = checkerboard;
    
    nvrhi::ComputeState state;
    state.bindings = { m_bindingSet };
    state.pipeline = m_computePipeline;
    commandList->setComputeState(state);

    for (int passIndex = 0; passIndex < c_NumFilterPasses; passIndex++)
    {
        constants.passIndex = passIndex;
        commandList->setPushConstants(&constants, sizeof(constants));

        commandList->dispatch(
            dm::div_ceil(view.GetViewExtent().width(), 8),
            dm::div_ceil(view.GetViewExtent().height(), 8),
            1);

        nvrhi::utils::TextureUavBarrier(commandList, m_gradientsTexture);
        commandList->commitBarriers();
    }

    commandList->endMarker();
}

int FilterGradientsPass::GetOutputBufferIndex()
{
    return c_NumFilterPasses & 1;
}
