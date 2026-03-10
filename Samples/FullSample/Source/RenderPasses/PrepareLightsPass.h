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

#pragma once

#include <donut/engine/SceneGraph.h>
#include <nvrhi/nvrhi.h>
#include <Rtxdi/DI/ReSTIRDI.h>
#include <memory>
#include <unordered_map>


namespace donut::engine
{
    class CommonRenderPasses;
    class ShaderFactory;
    class Scene;
    class Light;
}

class RtxdiResources;

class PrepareLightsPass
{
public:
    PrepareLightsPass(
        nvrhi::IDevice* device,
        std::shared_ptr<donut::engine::ShaderFactory> shaderFactory,
        std::shared_ptr<donut::engine::CommonRenderPasses> commonPasses,
        std::shared_ptr<donut::engine::Scene> scene,
        nvrhi::IBindingLayout* bindlessLayout);

    void CreatePipeline();
    void CreateBindingSet(RtxdiResources& resources);
    void CountLightsInScene(uint32_t& numEmissiveMeshes, uint32_t& numEmissiveTriangles);
    
    RTXDI_LightBufferParameters Process(
        nvrhi::ICommandList* commandList, 
        const rtxdi::ReSTIRDIContext& context, 
        const std::vector<std::shared_ptr<donut::engine::Light>>& sceneLights,
        bool enableImportanceSampledEnvironmentLight);

    uint32_t GetMaxLightsInBuffer() const;

private:
    nvrhi::DeviceHandle m_device;

    nvrhi::ShaderHandle m_computeShader;
    nvrhi::ComputePipelineHandle m_computePipeline;
    nvrhi::BindingLayoutHandle m_bindingLayout;
    nvrhi::BindingSetHandle m_bindingSet;
    nvrhi::BindingLayoutHandle m_bindlessLayout;

    nvrhi::BufferHandle m_taskBuffer;
    nvrhi::BufferHandle m_primitiveLightBuffer;
    nvrhi::BufferHandle m_lightIndexMappingBuffer;
    nvrhi::BufferHandle m_geometryInstanceToLightBuffer;
    nvrhi::TextureHandle m_localLightPdfTexture;

    uint32_t m_maxLightsInBuffer;
    bool m_oddFrame = false;

    std::shared_ptr<donut::engine::ShaderFactory> m_shaderFactory;
    std::shared_ptr<donut::engine::CommonRenderPasses> m_commonPasses;
    std::shared_ptr<donut::engine::Scene> m_scene;

    std::unordered_map<size_t, uint32_t> m_instanceLightBufferOffsets; // hash(instance*, geometryIndex) -> bufferOffset
    std::unordered_map<const donut::engine::Light*, uint32_t> m_primitiveLightBufferOffsets;
};
