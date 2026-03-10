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

#include "GlassPass.h"
#include "../RenderTargets.h"
#include "../Scene/Lights.h"

#include <donut/engine/CommonRenderPasses.h>
#include <donut/engine/ShaderFactory.h>
#include <donut/engine/View.h>
#include <nvrhi/utils.h>

#include "../Profiler.h"

using namespace donut::math;
#include "SharedShaderInclude/ShaderParameters.h"

using namespace donut::engine;


GlassPass::GlassPass(
    nvrhi::IDevice* device,
    std::shared_ptr<donut::engine::ShaderFactory> shaderFactory,
    std::shared_ptr<donut::engine::CommonRenderPasses> commonPasses,
    std::shared_ptr<donut::engine::Scene> scene,
    std::shared_ptr<Profiler> profiler,
    nvrhi::IBindingLayout* bindlessLayout)
    : m_device(device)
    , m_shaderFactory(shaderFactory)
    , m_commonPasses(commonPasses)
    , m_scene(scene)
    , m_bindlessLayout(bindlessLayout)
    , m_profiler(profiler)
{
    m_constantBuffer = m_device->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(GlassConstants), "GlassConstants", 16));

    nvrhi::BindingLayoutDesc globalBindingLayoutDesc;
    globalBindingLayoutDesc.visibility = nvrhi::ShaderType::Compute | nvrhi::ShaderType::AllRayTracing;
    globalBindingLayoutDesc.bindings = {
        nvrhi::BindingLayoutItem::RayTracingAccelStruct(0),
        nvrhi::BindingLayoutItem::StructuredBuffer_SRV(1),
        nvrhi::BindingLayoutItem::StructuredBuffer_SRV(2),
        nvrhi::BindingLayoutItem::StructuredBuffer_SRV(3),
        nvrhi::BindingLayoutItem::Texture_SRV(4),
        nvrhi::BindingLayoutItem::Texture_SRV(5),
        nvrhi::BindingLayoutItem::Texture_SRV(6),
        nvrhi::BindingLayoutItem::Sampler(0),
        nvrhi::BindingLayoutItem::Sampler(1),
        nvrhi::BindingLayoutItem::Texture_UAV(0),
        nvrhi::BindingLayoutItem::TypedBuffer_UAV(1),
        nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
        nvrhi::BindingLayoutItem::PushConstants(1, sizeof(PerPassConstants)),
    };

    m_bindingLayout = m_device->createBindingLayout(globalBindingLayoutDesc);
}

void GlassPass::CreatePipeline(bool useRayQuery)
{
    m_pass.Init(m_device, *m_shaderFactory, "app/GlassPass.hlsl", {}, useRayQuery, 16, m_bindingLayout, nullptr, m_bindlessLayout);
}

void GlassPass::CreateBindingSet(
    nvrhi::rt::IAccelStruct* topLevelAS,
    nvrhi::rt::IAccelStruct* prevTopLevelAS,
    const RenderTargets& renderTargets)
{
    for (int currentFrame = 0; currentFrame <= 1; currentFrame++)
    {
        nvrhi::BindingSetDesc bindingSetDesc;
        bindingSetDesc.bindings = {
            nvrhi::BindingSetItem::RayTracingAccelStruct(0, currentFrame ? topLevelAS : prevTopLevelAS),
            nvrhi::BindingSetItem::StructuredBuffer_SRV(1, m_scene->GetInstanceBuffer()),
            nvrhi::BindingSetItem::StructuredBuffer_SRV(2, m_scene->GetGeometryBuffer()),
            nvrhi::BindingSetItem::StructuredBuffer_SRV(3, m_scene->GetMaterialBuffer()),
            nvrhi::BindingSetItem::Texture_SRV(4, renderTargets.GBufferEmissive),
            nvrhi::BindingSetItem::Texture_SRV(5, renderTargets.GBufferDiffuseAlbedo),
            nvrhi::BindingSetItem::Texture_SRV(6, renderTargets.GBufferSpecularRough),
            nvrhi::BindingSetItem::Sampler(0, m_commonPasses->m_LinearWrapSampler),
            nvrhi::BindingSetItem::Sampler(1, m_commonPasses->m_LinearWrapSampler),
            nvrhi::BindingSetItem::Texture_UAV(0, renderTargets.HdrColor),
            nvrhi::BindingSetItem::TypedBuffer_UAV(1, m_profiler->GetRayCountBuffer()),
            nvrhi::BindingSetItem::ConstantBuffer(0, m_constantBuffer),
            nvrhi::BindingSetItem::PushConstants(1, sizeof(PerPassConstants)),
        };

        const nvrhi::BindingSetHandle bindingSet = m_device->createBindingSet(bindingSetDesc, m_bindingLayout);

        if (currentFrame)
            m_bindingSet = bindingSet;
        else
            m_prevBindingSet = bindingSet;
    }
}

void GlassPass::Render(
    nvrhi::ICommandList* commandList, 
    const donut::engine::IView& view,
    const EnvironmentLight& environmentLight,
    float normalMapScale,
    bool enableMaterialReadback,
    dm::int2 materialReadbackPosition,
    IndirectLightingMode indirectLightingMode)
{
    commandList->beginMarker("Glass");

    GlassConstants constants = {};
    view.FillPlanarViewConstants(constants.view);
    constants.enableEnvironmentMap = (environmentLight.textureIndex >= 0);
    constants.environmentMapTextureIndex = (environmentLight.textureIndex >= 0) ? environmentLight.textureIndex : 0;
    constants.environmentScale = environmentLight.radianceScale.x;
    constants.environmentRotation = environmentLight.rotation;
    constants.normalMapScale = normalMapScale;
    constants.materialReadbackBufferIndex = ProfilerSection::MaterialReadback * 2;
    constants.materialReadbackPosition = enableMaterialReadback ? materialReadbackPosition : int2(-1, -1);
    constants.indirectLightingMode = indirectLightingMode;
    commandList->writeBuffer(m_constantBuffer, &constants, sizeof(constants));

    PerPassConstants pushConstants{};
    pushConstants.rayCountBufferIndex = ProfilerSection::Glass;

    m_pass.Execute(commandList, view.GetViewExtent().width(), view.GetViewExtent().height(), 
        m_bindingSet, nullptr, m_scene->GetDescriptorTable(), &pushConstants, sizeof(pushConstants));

    commandList->endMarker();
}

void GlassPass::NextFrame()
{
    std::swap(m_bindingSet, m_prevBindingSet);
}
