/***************************************************************************
 # Copyright (c) 2020-2023, NVIDIA CORPORATION.  All rights reserved.
 #
 # NVIDIA CORPORATION and its licensors retain all intellectual property
 # and proprietary rights in and to this software, related documentation
 # and any modifications thereto.  Any use, reproduction, disclosure or
 # distribution of this software and related documentation without an express
 # license agreement from NVIDIA CORPORATION is strictly prohibited.
 **************************************************************************/

#pragma once

#if WITH_NRD

#include <NRD.h>
#include <nvrhi/nvrhi.h>
#include <unordered_map>
#include <donut/engine/BindingCache.h>
#include <donut/core/math/math.h>

class RenderTargets;

namespace donut::engine
{
    class PlanarView;
}

class NrdIntegration
{
public:
    NrdIntegration(nvrhi::IDevice* device, nrd::Denoiser denoiser);

    bool Initialize(uint32_t width, uint32_t height);
    bool IsAvailable() const;

    void RunDenoiserPasses(
        nvrhi::ICommandList* commandList,
        const RenderTargets& renderTargets, 
        const donut::engine::PlanarView& view, 
        const donut::engine::PlanarView& viewPrev,
        uint32_t frameIndex,
        bool enableConfidenceInputs,
        const void* denoiserSettings,
        float debug);

    const nrd::Denoiser GetDenoiser() const;

private:
    nvrhi::DeviceHandle m_device;
    bool m_initialized;
    nrd::Instance* m_instance;
    nrd::Denoiser m_denoiser;

    struct NrdPipeline
    {
        nvrhi::ShaderHandle Shader;
        nvrhi::BindingLayoutHandle BindingLayout;
        nvrhi::ComputePipelineHandle Pipeline;
    };

    nvrhi::BufferHandle m_constantBuffer;
    std::vector<NrdPipeline> m_pipelines;
    std::vector<nvrhi::SamplerHandle> m_samplers;
    std::vector<nvrhi::TextureHandle> m_permanentTextures;
    std::vector<nvrhi::TextureHandle> m_transientTextures;
    donut::engine::BindingCache m_bindingCache;
    dm::float2 m_pixelOffsetPrev;
};

#endif
