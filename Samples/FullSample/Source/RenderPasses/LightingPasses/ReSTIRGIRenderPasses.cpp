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

#include "ReSTIRGIRenderPasses.h"

#include <donut/core/math/math.h>
#include <donut/engine/View.h>
#include <donut/engine/ShaderFactory.h>
#include <nvrhi/utils.h>

#include "Rtxdi/GI/ReSTIRGI.h"
#include "Profiler.h"
#include "ProfilerSections.h"
using namespace donut::math;
#include "SharedShaderInclude/ShaderParameters.h"

ReSTIRGIRenderPasses::ReSTIRGIRenderPasses(std::shared_ptr<Profiler> profiler) :
    m_enableRayCounts(false),
    m_profiler(profiler),
    m_descriptorTable(nullptr)
{

}

ReSTIRGIRenderPasses::~ReSTIRGIRenderPasses()
{

}

void ReSTIRGIRenderPasses::LoadShaders(nvrhi::DeviceHandle device, donut::engine::ShaderFactory& shaderFactory, bool useRayQuery, nvrhi::BindingLayoutHandle bindingLayout, nvrhi::BindingLayoutHandle bindlessLayout)
{
    m_GITemporalResamplingPass.Init(device, shaderFactory, "app/LightingPasses/GI/TemporalResampling.hlsl", {}, useRayQuery, RTXDI_SCREEN_SPACE_GROUP_SIZE, bindingLayout, nullptr, bindlessLayout);
    m_GISpatialResamplingPass.Init(device, shaderFactory, "app/LightingPasses/GI/SpatialResampling.hlsl", {}, useRayQuery, RTXDI_SCREEN_SPACE_GROUP_SIZE, bindingLayout, nullptr, bindlessLayout);
    m_GIFusedResamplingPass.Init(device, shaderFactory, "app/LightingPasses/GI/FusedResampling.hlsl", {}, useRayQuery, RTXDI_SCREEN_SPACE_GROUP_SIZE, bindingLayout, nullptr, bindlessLayout);
    m_GIFinalShadingPass.Init(device, shaderFactory, "app/LightingPasses/GI/FinalShading.hlsl", {}, false, RTXDI_SCREEN_SPACE_GROUP_SIZE, bindingLayout, nullptr, bindlessLayout);
}

void ReSTIRGIRenderPasses::SetBindingSet(nvrhi::IBindingSet* bindingSet)
{
    m_bindingSet = bindingSet;
}

void ReSTIRGIRenderPasses::SetDescriptorTable(nvrhi::IDescriptorTable* descriptorTable)
{
    m_descriptorTable = descriptorTable;
}

static void ExecuteRayTracingPass(nvrhi::ICommandList* commandList,
    RayTracingPass& pass,
    bool enableRayCounts,
    const char* passName,
    dm::int2 dispatchSize,
    Profiler& profiler,
    ProfilerSection::Enum profilerSection,
    nvrhi::IDescriptorTable* descriptorTable,
    nvrhi::IBindingSet* bindingSet,
    nvrhi::IBindingSet* extraBindingSet = nullptr)
{
    commandList->beginMarker(passName);
    profiler.BeginSection(commandList, profilerSection);

    PerPassConstants pushConstants{};
    pushConstants.rayCountBufferIndex = enableRayCounts ? profilerSection : -1;

    pass.Execute(commandList, dispatchSize.x, dispatchSize.y, bindingSet, extraBindingSet, descriptorTable, &pushConstants, sizeof(pushConstants));

    profiler.EndSection(commandList, profilerSection);
    commandList->endMarker();
}

void ReSTIRGIRenderPasses::Render(nvrhi::ICommandList* commandList,
                                  const donut::engine::IView& view,
                                  rtxdi::ReSTIRGIContext& context,
                                  nvrhi::BufferHandle giReservoirBuffer)
{
    dm::int2 dispatchSize = {
        view.GetViewExtent().width(),
        view.GetViewExtent().height()
    };

    rtxdi::ReSTIRGI_ResamplingMode resamplingMode = context.GetResamplingMode();
    if (resamplingMode == rtxdi::ReSTIRGI_ResamplingMode::FusedSpatiotemporal)
    {
        nvrhi::utils::BufferUavBarrier(commandList, giReservoirBuffer);

        ExecuteRayTracingPass(commandList, m_GIFusedResamplingPass, m_enableRayCounts, "GIFusedResampling", dispatchSize, *m_profiler, ProfilerSection::GIFusedResampling, m_descriptorTable, m_bindingSet, nullptr);
    }
    else
    {
        if (resamplingMode == rtxdi::ReSTIRGI_ResamplingMode::Temporal ||
            resamplingMode == rtxdi::ReSTIRGI_ResamplingMode::TemporalAndSpatial)
        {
            nvrhi::utils::BufferUavBarrier(commandList, giReservoirBuffer);

            ExecuteRayTracingPass(commandList, m_GITemporalResamplingPass, m_enableRayCounts, "GITemporalResampling", dispatchSize, *m_profiler, ProfilerSection::GITemporalResampling, m_descriptorTable, m_bindingSet, nullptr);
        }

        if (resamplingMode == rtxdi::ReSTIRGI_ResamplingMode::Spatial ||
            resamplingMode == rtxdi::ReSTIRGI_ResamplingMode::TemporalAndSpatial)
        {
            nvrhi::utils::BufferUavBarrier(commandList, giReservoirBuffer);

            ExecuteRayTracingPass(commandList, m_GISpatialResamplingPass, m_enableRayCounts, "GISpatialResampling", dispatchSize, *m_profiler, ProfilerSection::GISpatialResampling, m_descriptorTable, m_bindingSet, nullptr);
        }
    }

    nvrhi::utils::BufferUavBarrier(commandList, giReservoirBuffer);

    ExecuteRayTracingPass(commandList, m_GIFinalShadingPass, m_enableRayCounts, "GIFinalShading", dispatchSize, *m_profiler, ProfilerSection::GIFinalShading, m_descriptorTable, m_bindingSet, nullptr);
}

void ReSTIRGIRenderPasses::EnableRayCounts(bool enable)
{
    m_enableRayCounts = enable;
}

bool ReSTIRGIRenderPasses::RayCountsEnabled() const
{
    return m_enableRayCounts;
}