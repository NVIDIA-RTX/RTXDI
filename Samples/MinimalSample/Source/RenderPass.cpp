/***************************************************************************
 # Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
 #
 # NVIDIA CORPORATION and its licensors retain all intellectual property
 # and proprietary rights in and to this software, related documentation
 # and any modifications thereto.  Any use, reproduction, disclosure or
 # distribution of this software and related documentation without an express
 # license agreement from NVIDIA CORPORATION is strictly prohibited.
 **************************************************************************/

#include "RenderPass.h"
#include "RenderTargets.h"
#include "RtxdiResources.h"

#include <donut/engine/Scene.h>
#include <donut/engine/CommonRenderPasses.h>
#include <donut/engine/ShaderFactory.h>
#include <donut/engine/View.h>
#include <donut/core/log.h>
#include <nvrhi/utils.h>
#include <Rtxdi/DI/ReSTIRDI.h>

using namespace donut::math;
#include "../Shaders/ShaderParameters.h"

using namespace donut::engine;

RTXDI_DIInitialSamplingParameters GetDefaultMinimalSampleInitialSamplingParams()
{
    RTXDI_DIInitialSamplingParameters iParams = {};
    iParams.numLocalLightSamples = 8;
    iParams.numBrdfSamples = 1;
    iParams.numInfiniteLightSamples = 0;
    iParams.numEnvironmentSamples = 0;
    iParams.brdfCutoff = 0.0f;
    iParams.brdfRayMinT = 0.0f;
    return iParams;
}

RTXDI_DITemporalResamplingParameters GetDefaultMinimalTemporalParams()
{
    RTXDI_DITemporalResamplingParameters params = rtxdi::GetDefaultReSTIRDITemporalResamplingParams();
    params.enableVisibilityShortcut = true;
    return params;
}

RTXDI_DISpatialResamplingParameters GetDefaultMinimalSpatialParams()
{
    RTXDI_DISpatialResamplingParameters params = rtxdi::GetDefaultReSTIRDISpatialResamplingParams();
    params.numSamples = 2;
    params.numDisocclusionBoostSamples = 2;
    params.enableMaterialSimilarityTest = false;
    return params;
}

RenderPass::RenderPass(
    nvrhi::IDevice* device, 
    std::shared_ptr<ShaderFactory> shaderFactory,
    std::shared_ptr<donut::engine::CommonRenderPasses> commonPasses,
    std::shared_ptr<donut::engine::Scene> scene,
    nvrhi::IBindingLayout* bindlessLayout
)
    : m_device(device)
    , m_bindlessLayout(bindlessLayout)
    , m_shaderFactory(std::move(shaderFactory))
    , m_commonPasses(std::move(commonPasses))
    , m_scene(std::move(scene))
{
    // The binding layout descriptor must match the binding set descriptor defined in CreateBindingSet(...) below

    nvrhi::BindingLayoutDesc globalBindingLayoutDesc;
    globalBindingLayoutDesc.visibility = nvrhi::ShaderType::Compute | nvrhi::ShaderType::AllRayTracing;
    globalBindingLayoutDesc.bindings = {
        nvrhi::BindingLayoutItem::Texture_SRV(0),
        nvrhi::BindingLayoutItem::Texture_SRV(1),
        nvrhi::BindingLayoutItem::Texture_SRV(2),
        nvrhi::BindingLayoutItem::Texture_SRV(3),
        nvrhi::BindingLayoutItem::Texture_SRV(4),

        nvrhi::BindingLayoutItem::RayTracingAccelStruct(30),
        nvrhi::BindingLayoutItem::StructuredBuffer_SRV(32),
        nvrhi::BindingLayoutItem::StructuredBuffer_SRV(33),
        nvrhi::BindingLayoutItem::StructuredBuffer_SRV(34),

        nvrhi::BindingLayoutItem::StructuredBuffer_SRV(20),
        nvrhi::BindingLayoutItem::TypedBuffer_SRV(21),
        nvrhi::BindingLayoutItem::StructuredBuffer_SRV(22),

        nvrhi::BindingLayoutItem::StructuredBuffer_UAV(0),
        nvrhi::BindingLayoutItem::Texture_UAV(1),
        nvrhi::BindingLayoutItem::Texture_UAV(2),
        nvrhi::BindingLayoutItem::Texture_UAV(3),
        nvrhi::BindingLayoutItem::Texture_UAV(4),
        nvrhi::BindingLayoutItem::Texture_UAV(5),
        nvrhi::BindingLayoutItem::Texture_UAV(6),
        nvrhi::BindingLayoutItem::Texture_UAV(7),
        
        nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
        nvrhi::BindingLayoutItem::Sampler(0),
        nvrhi::BindingLayoutItem::Sampler(1),
    };

    m_bindingLayout = m_device->createBindingLayout(globalBindingLayoutDesc);

    m_constantBuffer = m_device->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ResamplingConstants), "ResamplingConstants", 16));
}

void RenderPass::CreateBindingSet(
    nvrhi::rt::IAccelStruct* topLevelAS,
    const RenderTargets& renderTargets,
    const RtxdiResources& resources)
{
    assert(&renderTargets);
    assert(&resources);

    for (int currentFrame = 0; currentFrame <= 1; currentFrame++)
    {
        // This list must match the binding declarations in RtxdiApplicationBridge.hlsli

        nvrhi::BindingSetDesc bindingSetDesc;
        bindingSetDesc.bindings = {
            nvrhi::BindingSetItem::Texture_SRV(0, currentFrame ? renderTargets.PrevDepth : renderTargets.Depth),
            nvrhi::BindingSetItem::Texture_SRV(1, currentFrame ? renderTargets.PrevGBufferNormals : renderTargets.GBufferNormals),
            nvrhi::BindingSetItem::Texture_SRV(2, currentFrame ? renderTargets.PrevGBufferGeoNormals : renderTargets.GBufferGeoNormals),
            nvrhi::BindingSetItem::Texture_SRV(3, currentFrame ? renderTargets.PrevGBufferDiffuseAlbedo : renderTargets.GBufferDiffuseAlbedo),
            nvrhi::BindingSetItem::Texture_SRV(4, currentFrame ? renderTargets.PrevGBufferSpecularRough : renderTargets.GBufferSpecularRough),
            
            nvrhi::BindingSetItem::RayTracingAccelStruct(30, topLevelAS),
            nvrhi::BindingSetItem::StructuredBuffer_SRV(32, m_scene->GetInstanceBuffer()),
            nvrhi::BindingSetItem::StructuredBuffer_SRV(33, m_scene->GetGeometryBuffer()),
            nvrhi::BindingSetItem::StructuredBuffer_SRV(34, m_scene->GetMaterialBuffer()),

            nvrhi::BindingSetItem::StructuredBuffer_SRV(20, resources.LightDataBuffer),
            nvrhi::BindingSetItem::TypedBuffer_SRV(21, resources.NeighborOffsetsBuffer),
            nvrhi::BindingSetItem::StructuredBuffer_SRV(22, resources.GeometryInstanceToLightBuffer),

            nvrhi::BindingSetItem::StructuredBuffer_UAV(0, resources.LightReservoirBuffer),
            nvrhi::BindingSetItem::Texture_UAV(1, renderTargets.HdrColor),
            nvrhi::BindingSetItem::Texture_UAV(2, currentFrame ? renderTargets.Depth : renderTargets.PrevDepth),
            nvrhi::BindingSetItem::Texture_UAV(3, currentFrame ? renderTargets.GBufferNormals : renderTargets.PrevGBufferNormals),
            nvrhi::BindingSetItem::Texture_UAV(4, currentFrame ? renderTargets.GBufferGeoNormals : renderTargets.PrevGBufferGeoNormals),
            nvrhi::BindingSetItem::Texture_UAV(5, currentFrame ? renderTargets.GBufferDiffuseAlbedo : renderTargets.PrevGBufferDiffuseAlbedo),
            nvrhi::BindingSetItem::Texture_UAV(6, currentFrame ? renderTargets.GBufferSpecularRough : renderTargets.PrevGBufferSpecularRough),
            nvrhi::BindingSetItem::Texture_UAV(7, renderTargets.MotionVectors),
            
            nvrhi::BindingSetItem::ConstantBuffer(0, m_constantBuffer),
            nvrhi::BindingSetItem::Sampler(0, m_commonPasses->m_LinearWrapSampler),
            nvrhi::BindingSetItem::Sampler(1, m_commonPasses->m_LinearWrapSampler)
        };

        const nvrhi::BindingSetHandle bindingSet = m_device->createBindingSet(bindingSetDesc, m_bindingLayout);

        if (currentFrame)
            m_bindingSet = bindingSet;
        else
            m_prevBindingSet = bindingSet;
    }
    
    m_lightReservoirBuffer = resources.LightReservoirBuffer;
    m_currentGBufferTextures = { renderTargets.Depth, renderTargets.GBufferNormals,
        renderTargets.GBufferGeoNormals, renderTargets.GBufferDiffuseAlbedo, renderTargets.GBufferSpecularRough };
    m_previousGBufferTextures = { renderTargets.PrevDepth, renderTargets.PrevGBufferNormals,
        renderTargets.PrevGBufferGeoNormals, renderTargets.PrevGBufferDiffuseAlbedo, renderTargets.PrevGBufferSpecularRough };
    m_motionVectors = renderTargets.MotionVectors;
    m_shadingOutput = renderTargets.HdrColor;
}

void RenderPass::CreatePipeline()
{
    nvrhi::ComputePipelineDesc pipelineDesc;
    pipelineDesc.bindingLayouts = { m_bindingLayout, m_bindlessLayout };
    pipelineDesc.CS = m_shaderFactory->CreateShader("app/GBufferFill.hlsl", "main", nullptr, nvrhi::ShaderType::Compute);
    m_gBufferPipeline = m_device->createComputePipeline(pipelineDesc);
    pipelineDesc.CS = m_shaderFactory->CreateShader("app/InitialSampling.hlsl", "main", nullptr, nvrhi::ShaderType::Compute);
    m_initialSamplingPipeline = m_device->createComputePipeline(pipelineDesc);
    pipelineDesc.CS = m_shaderFactory->CreateShader("app/TemporalResampling.hlsl", "main", nullptr, nvrhi::ShaderType::Compute);
    m_temporalPipeline = m_device->createComputePipeline(pipelineDesc);
    pipelineDesc.CS = m_shaderFactory->CreateShader("app/SpatialResampling.hlsl", "main", nullptr, nvrhi::ShaderType::Compute);
    m_spatialPipeline = m_device->createComputePipeline(pipelineDesc);
    pipelineDesc.CS = m_shaderFactory->CreateShader("app/FinalShading.hlsl", "main", nullptr, nvrhi::ShaderType::Compute);
    m_finalShadingPipeline = m_device->createComputePipeline(pipelineDesc);
}

void RenderPass::Render(
    nvrhi::ICommandList* commandList,
    rtxdi::ReSTIRDIContext& context,
    const donut::engine::IView& view,
    const donut::engine::IView& prevView,
    const Settings& localSettings,
    const RTXDI_LightBufferParameters& lightBufferParams)
{
    ResamplingConstants constants = {};
    view.FillPlanarViewConstants(constants.view);
    prevView.FillPlanarViewConstants(constants.prevView);

    context.SetResamplingMode(localSettings.enableResampling ? rtxdi::ReSTIRDI_ResamplingMode::TemporalAndSpatial : rtxdi::ReSTIRDI_ResamplingMode::None);
    context.SetTemporalResamplingParameters(localSettings.temporalResamplingParams);
    context.SetSpatialResamplingParameters(localSettings.spatialResamplingParams);

    constants.initialSamplingParams = localSettings.initialSamplingParams;
    constants.temporalResamplingParams = context.GetTemporalResamplingParameters();
    constants.spatialResamplingParams = context.GetSpatialResamplingParameters();
    constants.restirDIReservoirBufferParams = context.GetReservoirBufferParameters();
    constants.lightBufferParams = lightBufferParams;
    constants.runtimeParams = context.GetRuntimeParams();

    constants.bufferIndices = context.GetBufferIndices();
    
    commandList->writeBuffer(m_constantBuffer, &constants, sizeof(constants));

    const uint32_t dispatchWidth = dm::div_ceil(view.GetViewExtent().width(), RTXDI_SCREEN_SPACE_GROUP_SIZE);
    const uint32_t dispatchHeight = dm::div_ceil(view.GetViewExtent().height(), RTXDI_SCREEN_SPACE_GROUP_SIZE);

    commandList->beginMarker("GBuffer Fill");

    nvrhi::ComputeState state;
    state.bindings = { m_bindingSet, m_scene->GetDescriptorTable() };
    state.pipeline = m_gBufferPipeline;
    commandList->setComputeState(state);

    commandList->dispatch(dispatchWidth, dispatchHeight);

    commandList->endMarker();

    // Complete the surface data before DI initial sampling reads it.
    for (const auto& texture : m_currentGBufferTextures)
    {
        commandList->setTextureState(texture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
    }
    commandList->setTextureState(m_motionVectors, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
    commandList->setTextureState(m_shadingOutput, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
    commandList->commitBarriers();

    commandList->beginMarker("Initial Sampling");
    state.pipeline = m_initialSamplingPipeline;
    commandList->setComputeState(state);
    commandList->dispatch(dispatchWidth, dispatchHeight);
    commandList->endMarker();

    commandList->setBufferState(m_lightReservoirBuffer, nvrhi::ResourceStates::UnorderedAccess);
    commandList->commitBarriers();

    if (localSettings.enableResampling)
    {
        commandList->beginMarker("Temporal Resampling");
        state.pipeline = m_temporalPipeline;
        commandList->setComputeState(state);
        commandList->dispatch(dispatchWidth, dispatchHeight);
        commandList->endMarker();

        // Spatial reuse reads neighboring pixels from the completed temporal pass.
        commandList->setBufferState(m_lightReservoirBuffer, nvrhi::ResourceStates::UnorderedAccess);
        commandList->commitBarriers();

        commandList->beginMarker("Spatial Resampling");
        state.pipeline = m_spatialPipeline;
        commandList->setComputeState(state);
        commandList->dispatch(dispatchWidth, dispatchHeight);
        commandList->endMarker();

        commandList->setBufferState(m_lightReservoirBuffer, nvrhi::ResourceStates::UnorderedAccess);
        commandList->commitBarriers();
    }

    commandList->beginMarker("Final Shading");
    state.pipeline = m_finalShadingPipeline;
    commandList->setComputeState(state);
    commandList->dispatch(dispatchWidth, dispatchHeight);
    commandList->endMarker();
}

void RenderPass::NextFrame()
{
    std::swap(m_bindingSet, m_prevBindingSet);
    std::swap(m_currentGBufferTextures, m_previousGBufferTextures);
}
