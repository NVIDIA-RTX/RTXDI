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

#include "RenderEnvironmentMapPass.h"
#include <donut/engine/ShaderFactory.h>
#include <nvrhi/utils.h>

#include <donut/core/math/math.h>
#include <donut/core/log.h>
#include <donut/render/SkyPass.h>

#include "PrepareLightsPass.h"
#include "donut/engine/DescriptorTableManager.h"
#include "donut/engine/SceneTypes.h"
using namespace donut::math;

#include "SharedShaderInclude/ShaderParameters.h"

RenderEnvironmentMapPass::RenderEnvironmentMapPass(
    nvrhi::IDevice* device,
    std::shared_ptr<donut::engine::ShaderFactory> shaderFactory,
    std::shared_ptr<donut::engine::DescriptorTableManager> descriptorTable,
    uint32_t textureWidth)
    : m_descriptorTable(std::move(descriptorTable))
{
    donut::log::debug("Initializing RenderEnvironmentMapPass...");

    nvrhi::TextureDesc destDesc;
    destDesc.width = textureWidth;
    destDesc.height = textureWidth / 2;
    destDesc.mipLevels = 1;
    destDesc.isUAV = true;
    destDesc.debugName = "ProceduralEnvironmentMap";
    destDesc.initialState = nvrhi::ResourceStates::ShaderResource;
    destDesc.keepInitialState = true;
    destDesc.format = nvrhi::Format::RGBA16_FLOAT;
    m_destinationTexture = device->createTexture(destDesc);

    nvrhi::BindingSetDesc bindingSetDesc;
    bindingSetDesc.bindings = {
        nvrhi::BindingSetItem::PushConstants(0, sizeof(RenderEnvironmentMapConstants)),
        nvrhi::BindingSetItem::Texture_UAV(0, m_destinationTexture)
    };

    nvrhi::BindingLayoutHandle bindingLayout;
    nvrhi::utils::CreateBindingSetAndLayout(device, nvrhi::ShaderType::Compute, 0,
        bindingSetDesc, bindingLayout, m_bindingSet);

    nvrhi::ShaderHandle shader = shaderFactory->CreateShader("app/RenderEnvironmentMap.hlsl", "main", nullptr, nvrhi::ShaderType::Compute);

    nvrhi::ComputePipelineDesc pipelineDesc;
    pipelineDesc.bindingLayouts = { bindingLayout };
    pipelineDesc.CS = shader;
    m_pipeline = device->createComputePipeline(pipelineDesc);

    m_destinationTextureIndex = m_descriptorTable->CreateDescriptor(nvrhi::BindingSetItem::Texture_SRV(0, m_destinationTexture));
}

RenderEnvironmentMapPass::~RenderEnvironmentMapPass()
{
    if (m_destinationTextureIndex >= 0)
    {
        m_descriptorTable->ReleaseDescriptor(m_destinationTextureIndex);
        m_destinationTextureIndex = -1;
    }
}

void RenderEnvironmentMapPass::Render(nvrhi::ICommandList* commandList, const donut::engine::DirectionalLight& light, const donut::render::SkyParameters& params)
{
    commandList->beginMarker("RenderEnvironmentMap");

    const auto& destDesc = m_destinationTexture->getDesc();

    nvrhi::ComputeState state;
    state.pipeline = m_pipeline;
    state.bindings = { m_bindingSet };
    commandList->setComputeState(state);

    RenderEnvironmentMapConstants constants{};
    constants.invTextureSize = { 1.f / float(destDesc.width), 1.f / float(destDesc.height) };
    donut::render::SkyPass::FillShaderParameters(light, params, constants.params);
    commandList->setPushConstants(&constants, sizeof(constants));

    commandList->dispatch(div_ceil(destDesc.width, 16), div_ceil(destDesc.height, 16), 1);
    
    commandList->endMarker();
}

nvrhi::ITexture* RenderEnvironmentMapPass::GetTexture() const
{
    return m_destinationTexture;
}

int RenderEnvironmentMapPass::GetTextureIndex() const
{
    return m_destinationTextureIndex;
}
