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

#include "ReSTIRDIRenderPasses.h"

#include <donut/core/math/math.h>
#include <donut/engine/View.h>
#include <donut/engine/ShaderFactory.h>
#include <nvrhi/utils.h>

#include "Rtxdi/DI/ReSTIRDI.h"
#include "Profiler.h"
#include "ProfilerSections.h"
using namespace donut::math;
#include "SharedShaderInclude/ShaderParameters.h"

ReSTIRDIRenderPasses::ReSTIRDIRenderPasses(std::shared_ptr<Profiler> profiler) :
    m_profiler(profiler)
{

}

ReSTIRDIRenderPasses::~ReSTIRDIRenderPasses()
{

}

void ReSTIRDIRenderPasses::LoadShaders(nvrhi::DeviceHandle device, donut::engine::ShaderFactory& shaderFactory, bool useRayQuery, const std::vector<donut::engine::ShaderMacro>& regirMacros, nvrhi::BindingLayoutHandle bindingLayout, nvrhi::BindingLayoutHandle bindlessLayout)
{
    // Resampling shaders
    m_generateInitialSamplesPass.Init(device, shaderFactory, "app/LightingPasses/DI/GenerateInitialSamples.hlsl", regirMacros, useRayQuery, RTXDI_SCREEN_SPACE_GROUP_SIZE, bindingLayout, nullptr, bindlessLayout);
    m_temporalResamplingPass.Init(device, shaderFactory, "app/LightingPasses/DI/TemporalResampling.hlsl", {}, useRayQuery, RTXDI_SCREEN_SPACE_GROUP_SIZE, bindingLayout, nullptr, bindlessLayout);
    m_spatialResamplingPass.Init(device, shaderFactory, "app/LightingPasses/DI/SpatialResampling.hlsl", {}, useRayQuery, RTXDI_SCREEN_SPACE_GROUP_SIZE, bindingLayout, nullptr, bindlessLayout);
    m_fusedResamplingPass.Init(device, shaderFactory, "app/LightingPasses/DI/FusedResampling.hlsl", regirMacros, useRayQuery, RTXDI_SCREEN_SPACE_GROUP_SIZE, bindingLayout, nullptr, bindlessLayout);
    m_shadeSamplesPass.Init(device, shaderFactory, "app/LightingPasses/DI/ShadeSamples.hlsl", regirMacros, false, RTXDI_SCREEN_SPACE_GROUP_SIZE, bindingLayout, nullptr, bindlessLayout);
    m_gradientsPass.Init(device, shaderFactory, "app/DenoisingPasses/ComputeGradients.hlsl", {}, useRayQuery, RTXDI_SCREEN_SPACE_GROUP_SIZE, bindingLayout, nullptr, bindlessLayout);
}

void ReSTIRDIRenderPasses::SetBindingSet(nvrhi::IBindingSet* bindingSet)
{
    m_bindingSet = bindingSet;
}

void ReSTIRDIRenderPasses::SetDescriptorTable(nvrhi::IDescriptorTable* descriptorTable)
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

void ReSTIRDIRenderPasses::Render(nvrhi::ICommandList* commandList,
    const donut::engine::IView& view,
    rtxdi::ReSTIRDIContext& context,
    nvrhi::BufferHandle diReservoirBuffer)
{
    dm::int2 dispatchSize = {
    view.GetViewExtent().width(),
    view.GetViewExtent().height()
    };

    if (context.GetStaticParameters().CheckerboardSamplingMode != rtxdi::CheckerboardMode::Off)
        dispatchSize.x /= 2;

    // Run the lighting passes in the necessary sequence: one fused kernel or multiple separate passes.
    //
    // Note: the below code places explicit UAV barriers between subsequent passes
    // because NVRHI misses them, as the binding sets are exactly the same between these passes.
    // That equality makes NVRHI take a shortcut for performance and it doesn't look at bindings at all.

    if (context.GetResamplingMode() == rtxdi::ReSTIRDI_ResamplingMode::FusedSpatiotemporal)
    {
        nvrhi::utils::BufferUavBarrier(commandList, diReservoirBuffer);

        ExecuteRayTracingPass(commandList, m_fusedResamplingPass, m_enableRayCounts, "DIFusedResampling", dispatchSize, *m_profiler, ProfilerSection::Shading, m_descriptorTable, m_bindingSet);
    }
    else
    {
        ExecuteRayTracingPass(commandList, m_generateInitialSamplesPass, m_enableRayCounts, "DIGenerateInitialSamples", dispatchSize, *m_profiler, ProfilerSection::InitialSamples, m_descriptorTable, m_bindingSet);

        if (context.GetResamplingMode() == rtxdi::ReSTIRDI_ResamplingMode::Temporal || context.GetResamplingMode() == rtxdi::ReSTIRDI_ResamplingMode::TemporalAndSpatial)
        {
            nvrhi::utils::BufferUavBarrier(commandList, diReservoirBuffer);

            ExecuteRayTracingPass(commandList, m_temporalResamplingPass, m_enableRayCounts, "DITemporalResampling", dispatchSize, *m_profiler, ProfilerSection::TemporalResampling, m_descriptorTable, m_bindingSet);
        }

        if (context.GetResamplingMode() == rtxdi::ReSTIRDI_ResamplingMode::Spatial || context.GetResamplingMode() == rtxdi::ReSTIRDI_ResamplingMode::TemporalAndSpatial)
        {
            nvrhi::utils::BufferUavBarrier(commandList, diReservoirBuffer);

            ExecuteRayTracingPass(commandList, m_spatialResamplingPass, m_enableRayCounts, "DISpatialResampling", dispatchSize, *m_profiler, ProfilerSection::SpatialResampling, m_descriptorTable, m_bindingSet);
        }

        nvrhi::utils::BufferUavBarrier(commandList, diReservoirBuffer);

        ExecuteRayTracingPass(commandList, m_shadeSamplesPass, m_enableRayCounts, "DIShadeSamples", dispatchSize, *m_profiler, ProfilerSection::Shading, m_descriptorTable, m_bindingSet);
    }

    if (m_enableGradients)
    {
        nvrhi::utils::BufferUavBarrier(commandList, diReservoirBuffer);
        auto gradientsDispatchSize = (dispatchSize + RTXDI_GRAD_FACTOR - 1) / RTXDI_GRAD_FACTOR;
        ExecuteRayTracingPass(commandList, m_gradientsPass, m_enableRayCounts, "DIGradients", gradientsDispatchSize, *m_profiler, ProfilerSection::Gradients, m_descriptorTable, m_bindingSet);
    }
}

void ReSTIRDIRenderPasses::EnableGradients(bool enable)
{
    m_enableGradients = enable;
}

bool ReSTIRDIRenderPasses::GradientsEnabled() const
{
    return m_enableGradients;
}

void ReSTIRDIRenderPasses::EnableRayCounts(bool enable)
{
    m_enableRayCounts = enable;
}

bool ReSTIRDIRenderPasses::RayCountsEnabled() const
{
    return m_enableRayCounts;
}
