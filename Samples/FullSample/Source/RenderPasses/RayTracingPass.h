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

#pragma once

#include <nvrhi/nvrhi.h>

namespace donut::engine
{
    class ShaderFactory;
    struct ShaderMacro;
}


struct RayTracingPass
{
    nvrhi::ShaderHandle ComputeShader;
    nvrhi::ComputePipelineHandle ComputePipeline;

    nvrhi::ShaderLibraryHandle ShaderLibrary;
    nvrhi::rt::PipelineHandle RayTracingPipeline;
    nvrhi::rt::ShaderTableHandle ShaderTable;

    uint32_t ComputeGroupSize = 0;

    bool UseRayQuery = false;

    bool Init(
        nvrhi::IDevice* device,
        donut::engine::ShaderFactory& shaderFactory,
        const char* shaderName,
        const std::vector<donut::engine::ShaderMacro>& extraMacros,
        bool useRayQuery,
        uint32_t computeGroupSize,
        nvrhi::IBindingLayout* bindingLayout,
        nvrhi::IBindingLayout* extraBindingLayout,
        nvrhi::IBindingLayout* bindlessLayout,
        int32_t HLSLExtensionsUAVSlot = -1);

    void Execute(
        nvrhi::ICommandList* commandList,
        int width,
        int height,
        nvrhi::IBindingSet* bindingSet,
        nvrhi::IBindingSet* extraBindingSet,
        nvrhi::IDescriptorTable* descriptorTable,
        const void* pushConstants = nullptr,
        size_t pushConstantSize = 0);
};