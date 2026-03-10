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
#include <memory>

namespace donut::engine
{
    class ShaderFactory;
}

class GenerateMipsPass
{
public:
    GenerateMipsPass(
        nvrhi::IDevice* device,
        std::shared_ptr<donut::engine::ShaderFactory> shaderFactory,
        nvrhi::ITexture* sourceEnvironmentMap,
        nvrhi::ITexture* destinationTexture);
    
    void Process(nvrhi::ICommandList* commandList);

private:
    nvrhi::ComputePipelineHandle m_pipeline;
    nvrhi::BindingSetHandle m_bindingSet;
    nvrhi::TextureHandle m_sourceTexture;
    nvrhi::TextureHandle m_destinationTexture;
};
