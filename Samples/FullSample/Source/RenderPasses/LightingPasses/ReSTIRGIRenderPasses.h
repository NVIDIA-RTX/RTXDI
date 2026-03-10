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

#pragma once

#include <memory>
#include <vector>
#include "RenderPasses/RayTracingPass.h"

#include <nvrhi/nvrhi.h>

namespace donut::engine
{
    class IView;
}

namespace nvrhi
{
    class IBindingSet;
    class ICommandList;
    class IDescriptorTable;
}

namespace rtxdi
{
    class ReSTIRGIContext;
}

class Profiler;

class ReSTIRGIRenderPasses
{
public:
    ReSTIRGIRenderPasses(std::shared_ptr<Profiler> profiler);
    ~ReSTIRGIRenderPasses();

    void LoadShaders(nvrhi::DeviceHandle device, donut::engine::ShaderFactory& shaderFactory, bool useRayQuery, nvrhi::BindingLayoutHandle bindingLayout, nvrhi::BindingLayoutHandle bindlessLayout);

    // Updated when resources change (screen resize or scene change)
    void SetBindingSet(nvrhi::IBindingSet* bindingSet);

    // Updated when scene changes
    void SetDescriptorTable(nvrhi::IDescriptorTable* descriptorTable);

    void Render(nvrhi::ICommandList* commandList,
        const donut::engine::IView& view,
        rtxdi::ReSTIRGIContext& context,
        nvrhi::BufferHandle giReservoirBuffer);

    void EnableRayCounts(bool enable);
    bool RayCountsEnabled() const;

private:
    bool m_enableRayCounts;

    std::shared_ptr<Profiler> m_profiler;

    nvrhi::BindingSetHandle m_bindingSet;
    nvrhi::IDescriptorTable* m_descriptorTable;

    RayTracingPass m_GITemporalResamplingPass;
    RayTracingPass m_GISpatialResamplingPass;
    RayTracingPass m_GIFusedResamplingPass;
    RayTracingPass m_GIFinalShadingPass;
};
