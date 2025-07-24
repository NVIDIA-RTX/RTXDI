/***************************************************************************
 # Copyright (c) 2020-2023, NVIDIA CORPORATION.  All rights reserved.
 #
 # NVIDIA CORPORATION and its licensors retain all intellectual property
 # and proprietary rights in and to this software, related documentation
 # and any modifications thereto.  Any use, reproduction, disclosure or
 # distribution of this software and related documentation without an express
 # license agreement from NVIDIA CORPORATION is strictly prohibited.
 **************************************************************************/

#include "LightingPasses.h"
#include "../RenderTargets.h"
#include "../RtxdiResources.h"
#include "../Profiler.h"
#include "../SampleScene.h"
#include "GBufferPass.h"

#include <donut/engine/Scene.h>
#include <donut/engine/CommonRenderPasses.h>
#include <donut/engine/ShaderFactory.h>
#include <donut/engine/View.h>
#include <donut/core/log.h>
#include <nvrhi/utils.h>
#include <Rtxdi/ImportanceSamplingContext.h>

#include <utility>

#if WITH_NRD
#include <NRD.h>
#endif

using namespace donut::math;
#include "../../shaders/ShaderParameters.h"

using namespace donut::engine;

BRDFPathTracing_MaterialOverrideParameters GetDefaultBRDFPathTracingMaterialOverrideParams()
{
    BRDFPathTracing_MaterialOverrideParameters params = {};
    params.metalnessOverride = 0.5;
    params.minSecondaryRoughness = 0.5;
    params.roughnessOverride = 0.5;
    return params;
}

BRDFPathTracing_SecondarySurfaceReSTIRDIParameters GetDefaultBRDFPathTracingSecondarySurfaceReSTIRDIParams()
{
    BRDFPathTracing_SecondarySurfaceReSTIRDIParameters params = {};

    params.initialSamplingParams.localLightSamplingMode = ReSTIRDI_LocalLightSamplingMode::ReGIR_RIS;
    params.initialSamplingParams.numPrimaryLocalLightSamples = 2;
    params.initialSamplingParams.numPrimaryInfiniteLightSamples = 1;
    params.initialSamplingParams.numPrimaryEnvironmentSamples = 1;
    params.initialSamplingParams.numPrimaryBrdfSamples = 0;
    params.initialSamplingParams.brdfCutoff = 0;
    params.initialSamplingParams.enableInitialVisibility = false;

    params.spatialResamplingParams.numSpatialSamples = 1;
    params.spatialResamplingParams.spatialSamplingRadius = 4.0f;
    params.spatialResamplingParams.spatialBiasCorrection = ReSTIRDI_SpatialBiasCorrectionMode::Basic;
    params.spatialResamplingParams.numDisocclusionBoostSamples = 0; // Disabled
    params.spatialResamplingParams.spatialDepthThreshold = 0.1f;
    params.spatialResamplingParams.spatialNormalThreshold = 0.9f;

    return params;
}

BRDFPathTracing_Parameters GetDefaultBRDFPathTracingParams()
{
    BRDFPathTracing_Parameters params;
    params.enableIndirectEmissiveSurfaces = false;
    params.enableReSTIRGI = false;
    params.materialOverrideParams = GetDefaultBRDFPathTracingMaterialOverrideParams();
    params.secondarySurfaceReSTIRDIParams = GetDefaultBRDFPathTracingSecondarySurfaceReSTIRDIParams();
    return params;
}

LightingPasses::LightingPasses(
    nvrhi::IDevice* device, 
    std::shared_ptr<ShaderFactory> shaderFactory,
    std::shared_ptr<donut::engine::CommonRenderPasses> commonPasses,
    std::shared_ptr<donut::engine::Scene> scene,
    std::shared_ptr<Profiler> profiler,
    nvrhi::IBindingLayout* bindlessLayout
)
    : m_device(device)
    , m_bindlessLayout(bindlessLayout)
    , m_shaderFactory(std::move(shaderFactory))
    , m_commonPasses(std::move(commonPasses))
    , m_scene(std::move(scene))
    , m_profiler(std::move(profiler))
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
        nvrhi::BindingLayoutItem::Texture_SRV(5),
        nvrhi::BindingLayoutItem::Texture_SRV(6),
        nvrhi::BindingLayoutItem::Texture_SRV(7),
        nvrhi::BindingLayoutItem::Texture_SRV(8),
        nvrhi::BindingLayoutItem::Texture_SRV(9),
        nvrhi::BindingLayoutItem::Texture_SRV(10),
        nvrhi::BindingLayoutItem::Texture_SRV(11),
        nvrhi::BindingLayoutItem::Texture_SRV(12),

        nvrhi::BindingLayoutItem::RayTracingAccelStruct(30),
        nvrhi::BindingLayoutItem::RayTracingAccelStruct(31),
        nvrhi::BindingLayoutItem::StructuredBuffer_SRV(32),
        nvrhi::BindingLayoutItem::StructuredBuffer_SRV(33),
        nvrhi::BindingLayoutItem::StructuredBuffer_SRV(34),

        nvrhi::BindingLayoutItem::StructuredBuffer_SRV(20),
        nvrhi::BindingLayoutItem::TypedBuffer_SRV(21),
        nvrhi::BindingLayoutItem::TypedBuffer_SRV(22),
        nvrhi::BindingLayoutItem::Texture_SRV(23),
        nvrhi::BindingLayoutItem::Texture_SRV(24),
        nvrhi::BindingLayoutItem::StructuredBuffer_SRV(25),

        nvrhi::BindingLayoutItem::StructuredBuffer_UAV(0),
        nvrhi::BindingLayoutItem::Texture_UAV(1),
        nvrhi::BindingLayoutItem::Texture_UAV(2),
        nvrhi::BindingLayoutItem::Texture_UAV(3),
        nvrhi::BindingLayoutItem::Texture_UAV(4),
        nvrhi::BindingLayoutItem::Texture_UAV(5),
        nvrhi::BindingLayoutItem::StructuredBuffer_UAV(6),

        nvrhi::BindingLayoutItem::TypedBuffer_UAV(10),
        nvrhi::BindingLayoutItem::TypedBuffer_UAV(11),
        nvrhi::BindingLayoutItem::TypedBuffer_UAV(12),
        nvrhi::BindingLayoutItem::StructuredBuffer_UAV(13),

        nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
        nvrhi::BindingLayoutItem::PushConstants(1, sizeof(PerPassConstants)),
        nvrhi::BindingLayoutItem::Sampler(0),
        nvrhi::BindingLayoutItem::Sampler(1),
    };

    m_bindingLayout = m_device->createBindingLayout(globalBindingLayoutDesc);

    m_constantBuffer = m_device->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ResamplingConstants), "ResamplingConstants", 16));
}

void LightingPasses::CreateBindingSet(
    nvrhi::rt::IAccelStruct* topLevelAS,
    nvrhi::rt::IAccelStruct* prevTopLevelAS,
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
            nvrhi::BindingSetItem::Texture_SRV(0, currentFrame ? renderTargets.Depth : renderTargets.PrevDepth),
            nvrhi::BindingSetItem::Texture_SRV(1, currentFrame ? renderTargets.GBufferNormals : renderTargets.PrevGBufferNormals),
            nvrhi::BindingSetItem::Texture_SRV(2, currentFrame ? renderTargets.GBufferGeoNormals : renderTargets.PrevGBufferGeoNormals),
            nvrhi::BindingSetItem::Texture_SRV(3, currentFrame ? renderTargets.GBufferDiffuseAlbedo : renderTargets.PrevGBufferDiffuseAlbedo),
            nvrhi::BindingSetItem::Texture_SRV(4, currentFrame ? renderTargets.GBufferSpecularRough : renderTargets.PrevGBufferSpecularRough),
            nvrhi::BindingSetItem::Texture_SRV(5, currentFrame ? renderTargets.PrevDepth : renderTargets.Depth),
            nvrhi::BindingSetItem::Texture_SRV(6, currentFrame ? renderTargets.PrevGBufferNormals : renderTargets.GBufferNormals),
            nvrhi::BindingSetItem::Texture_SRV(7, currentFrame ? renderTargets.PrevGBufferGeoNormals : renderTargets.GBufferGeoNormals),
            nvrhi::BindingSetItem::Texture_SRV(8, currentFrame ? renderTargets.PrevGBufferDiffuseAlbedo : renderTargets.GBufferDiffuseAlbedo),
            nvrhi::BindingSetItem::Texture_SRV(9, currentFrame ? renderTargets.PrevGBufferSpecularRough : renderTargets.GBufferSpecularRough),
            nvrhi::BindingSetItem::Texture_SRV(10, currentFrame ? renderTargets.PrevRestirLuminance : renderTargets.RestirLuminance),
            nvrhi::BindingSetItem::Texture_SRV(11, renderTargets.MotionVectors),
            nvrhi::BindingSetItem::Texture_SRV(12, renderTargets.NormalRoughness),
            
            nvrhi::BindingSetItem::RayTracingAccelStruct(30, currentFrame ? topLevelAS : prevTopLevelAS),
            nvrhi::BindingSetItem::RayTracingAccelStruct(31, currentFrame ? prevTopLevelAS : topLevelAS),
            nvrhi::BindingSetItem::StructuredBuffer_SRV(32, m_scene->GetInstanceBuffer()),
            nvrhi::BindingSetItem::StructuredBuffer_SRV(33, m_scene->GetGeometryBuffer()),
            nvrhi::BindingSetItem::StructuredBuffer_SRV(34, m_scene->GetMaterialBuffer()),

            nvrhi::BindingSetItem::StructuredBuffer_SRV(20, resources.LightDataBuffer),
            nvrhi::BindingSetItem::TypedBuffer_SRV(21, resources.NeighborOffsetsBuffer),
            nvrhi::BindingSetItem::TypedBuffer_SRV(22, resources.LightIndexMappingBuffer),
            nvrhi::BindingSetItem::Texture_SRV(23, resources.EnvironmentPdfTexture),
            nvrhi::BindingSetItem::Texture_SRV(24, resources.LocalLightPdfTexture),
            nvrhi::BindingSetItem::StructuredBuffer_SRV(25, resources.GeometryInstanceToLightBuffer),

            nvrhi::BindingSetItem::StructuredBuffer_UAV(0, resources.LightReservoirBuffer),
            nvrhi::BindingSetItem::Texture_UAV(1, renderTargets.DiffuseLighting),
            nvrhi::BindingSetItem::Texture_UAV(2, renderTargets.SpecularLighting),
            nvrhi::BindingSetItem::Texture_UAV(3, renderTargets.TemporalSamplePositions),
            nvrhi::BindingSetItem::Texture_UAV(4, renderTargets.Gradients),
            nvrhi::BindingSetItem::Texture_UAV(5, currentFrame ? renderTargets.RestirLuminance : renderTargets.PrevRestirLuminance),
            nvrhi::BindingSetItem::StructuredBuffer_UAV(6, resources.GIReservoirBuffer),

            nvrhi::BindingSetItem::TypedBuffer_UAV(10, resources.RisBuffer),
            nvrhi::BindingSetItem::TypedBuffer_UAV(11, resources.RisLightDataBuffer),
            nvrhi::BindingSetItem::TypedBuffer_UAV(12, m_profiler->GetRayCountBuffer()),
            nvrhi::BindingSetItem::StructuredBuffer_UAV(13, resources.SecondaryGBuffer),

            nvrhi::BindingSetItem::ConstantBuffer(0, m_constantBuffer),
            nvrhi::BindingSetItem::PushConstants(1, sizeof(PerPassConstants)),
            nvrhi::BindingSetItem::Sampler(0, m_commonPasses->m_LinearWrapSampler),
            nvrhi::BindingSetItem::Sampler(1, m_commonPasses->m_LinearWrapSampler)
        };

        const nvrhi::BindingSetHandle bindingSet = m_device->createBindingSet(bindingSetDesc, m_bindingLayout);

        if (currentFrame)
            m_bindingSet = bindingSet;
        else
            m_prevBindingSet = bindingSet;
    }

    const auto& environmentPdfDesc = resources.EnvironmentPdfTexture->getDesc();
    m_environmentPdfTextureSize.x = environmentPdfDesc.width;
    m_environmentPdfTextureSize.y = environmentPdfDesc.height;
    
    const auto& localLightPdfDesc = resources.LocalLightPdfTexture->getDesc();
    m_localLightPdfTextureSize.x = localLightPdfDesc.width;
    m_localLightPdfTextureSize.y = localLightPdfDesc.height;

    m_lightReservoirBuffer = resources.LightReservoirBuffer;
    m_secondarySurfaceBuffer = resources.SecondaryGBuffer;
    m_GIReservoirBuffer = resources.GIReservoirBuffer;
}

void LightingPasses::CreateComputePass(ComputePass& pass, const char* shaderName, const std::vector<donut::engine::ShaderMacro>& macros)
{
    donut::log::debug("Initializing ComputePass %s...", shaderName);

    pass.Shader = m_shaderFactory->CreateShader(shaderName, "main", &macros, nvrhi::ShaderType::Compute);

    nvrhi::ComputePipelineDesc pipelineDesc;
    pipelineDesc.bindingLayouts = { m_bindingLayout, m_bindlessLayout };
    pipelineDesc.CS = pass.Shader;
    pass.Pipeline = m_device->createComputePipeline(pipelineDesc);
}

void LightingPasses::ExecuteComputePass(nvrhi::ICommandList* commandList, ComputePass& pass, const char* passName, dm::int2 dispatchSize, ProfilerSection::Enum profilerSection)
{
    commandList->beginMarker(passName);
    m_profiler->BeginSection(commandList, profilerSection);

    nvrhi::ComputeState state;
    state.bindings = { m_bindingSet, m_scene->GetDescriptorTable() };
    state.pipeline = pass.Pipeline;
    commandList->setComputeState(state);

    PerPassConstants pushConstants{};
    pushConstants.rayCountBufferIndex = -1;
    commandList->setPushConstants(&pushConstants, sizeof(pushConstants));

    commandList->dispatch(dispatchSize.x, dispatchSize.y, 1);

    m_profiler->EndSection(commandList, profilerSection);
    commandList->endMarker();
}

void LightingPasses::ExecuteRayTracingPass(nvrhi::ICommandList* commandList, RayTracingPass& pass, bool enableRayCounts, const char* passName, dm::int2 dispatchSize, ProfilerSection::Enum profilerSection, nvrhi::IBindingSet* extraBindingSet)
{
    commandList->beginMarker(passName);
    m_profiler->BeginSection(commandList, profilerSection);

    PerPassConstants pushConstants{};
    pushConstants.rayCountBufferIndex = enableRayCounts ? profilerSection : -1;
    
    pass.Execute(commandList, dispatchSize.x, dispatchSize.y, m_bindingSet, extraBindingSet, m_scene->GetDescriptorTable(), &pushConstants, sizeof(pushConstants));
    
    m_profiler->EndSection(commandList, profilerSection);
    commandList->endMarker();
}

donut::engine::ShaderMacro LightingPasses::GetRegirMacro(const rtxdi::ReGIRStaticParameters& regirStaticParams)
{
    std::string regirMode;

    switch (regirStaticParams.Mode)
    {
    case rtxdi::ReGIRMode::Disabled:
        regirMode = "RTXDI_REGIR_DISABLED";
        break;
    case rtxdi::ReGIRMode::Grid:
        regirMode = "RTXDI_REGIR_GRID";
        break;
    case rtxdi::ReGIRMode::Onion:
        regirMode = "RTXDI_REGIR_ONION";
        break;
    }

    return { "RTXDI_REGIR_MODE", regirMode };
}

void LightingPasses::CreatePresamplingPipelines()
{
    CreateComputePass(m_presampleLightsPass, "app/LightingPasses/Presampling/PresampleLights.hlsl", {});
    CreateComputePass(m_presampleEnvironmentMapPass, "app/LightingPasses/Presampling/PresampleEnvironmentMap.hlsl", {});
}

void LightingPasses::CreateReGIRPipeline(const rtxdi::ReGIRStaticParameters& regirStaticParams, const std::vector<donut::engine::ShaderMacro>& regirMacros)
{
    if (regirStaticParams.Mode != rtxdi::ReGIRMode::Disabled)
    {
        CreateComputePass(m_presampleReGIR, "app/LightingPasses/Presampling/PresampleReGIR.hlsl", regirMacros);
    }
}

void LightingPasses::CreateReSTIRDIPipelines(const std::vector<donut::engine::ShaderMacro>& regirMacros, bool useRayQuery)
{
    m_generateInitialSamplesPass.Init(m_device, *m_shaderFactory, "app/LightingPasses/DI/GenerateInitialSamples.hlsl", regirMacros, useRayQuery, RTXDI_SCREEN_SPACE_GROUP_SIZE, m_bindingLayout, nullptr, m_bindlessLayout);
    m_temporalResamplingPass.Init(m_device, *m_shaderFactory, "app/LightingPasses/DI/TemporalResampling.hlsl", {}, useRayQuery, RTXDI_SCREEN_SPACE_GROUP_SIZE, m_bindingLayout, nullptr, m_bindlessLayout);
    m_spatialResamplingPass.Init(m_device, *m_shaderFactory, "app/LightingPasses/DI/SpatialResampling.hlsl", {}, useRayQuery, RTXDI_SCREEN_SPACE_GROUP_SIZE, m_bindingLayout, nullptr, m_bindlessLayout);
    m_shadeSamplesPass.Init(m_device, *m_shaderFactory, "app/LightingPasses/DI/ShadeSamples.hlsl", regirMacros, useRayQuery, RTXDI_SCREEN_SPACE_GROUP_SIZE, m_bindingLayout, nullptr, m_bindlessLayout);
    m_brdfRayTracingPass.Init(m_device, *m_shaderFactory, "app/LightingPasses/BrdfRayTracing.hlsl", {}, useRayQuery, RTXDI_SCREEN_SPACE_GROUP_SIZE, m_bindingLayout, nullptr, m_bindlessLayout);
    m_shadeSecondarySurfacesPass.Init(m_device, *m_shaderFactory, "app/LightingPasses/ShadeSecondarySurfaces.hlsl", regirMacros, useRayQuery, RTXDI_SCREEN_SPACE_GROUP_SIZE, m_bindingLayout, nullptr, m_bindlessLayout);
    m_fusedResamplingPass.Init(m_device, *m_shaderFactory, "app/LightingPasses/DI/FusedResampling.hlsl", regirMacros, useRayQuery, RTXDI_SCREEN_SPACE_GROUP_SIZE, m_bindingLayout, nullptr, m_bindlessLayout);
    m_gradientsPass.Init(m_device, *m_shaderFactory, "app/DenoisingPasses/ComputeGradients.hlsl", {}, useRayQuery, RTXDI_SCREEN_SPACE_GROUP_SIZE, m_bindingLayout, nullptr, m_bindlessLayout);
}

void LightingPasses::CreateReSTIRGIPipelines(bool useRayQuery)
{
    m_GITemporalResamplingPass.Init(m_device, *m_shaderFactory, "app/LightingPasses/GI/TemporalResampling.hlsl", {}, useRayQuery, RTXDI_SCREEN_SPACE_GROUP_SIZE, m_bindingLayout, nullptr, m_bindlessLayout);
    m_GISpatialResamplingPass.Init(m_device, *m_shaderFactory, "app/LightingPasses/GI/SpatialResampling.hlsl", {}, useRayQuery, RTXDI_SCREEN_SPACE_GROUP_SIZE, m_bindingLayout, nullptr, m_bindlessLayout);
    m_GIFusedResamplingPass.Init(m_device, *m_shaderFactory, "app/LightingPasses/GI/FusedResampling.hlsl", {}, useRayQuery, RTXDI_SCREEN_SPACE_GROUP_SIZE, m_bindingLayout, nullptr, m_bindlessLayout);
    m_GIFinalShadingPass.Init(m_device, *m_shaderFactory, "app/LightingPasses/GI/FinalShading.hlsl", {}, useRayQuery, RTXDI_SCREEN_SPACE_GROUP_SIZE, m_bindingLayout, nullptr, m_bindlessLayout);
}

void LightingPasses::CreatePipelines(const rtxdi::ReGIRStaticParameters& regirStaticParams, bool useRayQuery)
{
    std::vector<donut::engine::ShaderMacro> regirMacros = {
        GetRegirMacro(regirStaticParams)
    };

    CreatePresamplingPipelines();
    CreateReGIRPipeline(regirStaticParams, regirMacros);
    CreateReSTIRDIPipelines(regirMacros, useRayQuery);
    CreateReSTIRGIPipelines(useRayQuery);
}

#if WITH_NRD
static void NrdHitDistanceParamsToFloat4(const nrd::HitDistanceParameters* params, dm::float4& out)
{
    assert(params);
    out.x = params->A;
    out.y = params->B;
    out.z = params->C;
    out.w = params->D;
}
#endif

void FillReSTIRDIConstants(ReSTIRDI_Parameters& params, const rtxdi::ReSTIRDIContext& restirDIContext, const RTXDI_LightBufferParameters& lightBufferParameters)
{
    params.reservoirBufferParams = restirDIContext.GetReservoirBufferParameters();
    params.bufferIndices = restirDIContext.GetBufferIndices();
    params.initialSamplingParams = restirDIContext.GetInitialSamplingParameters();
    params.initialSamplingParams.environmentMapImportanceSampling = lightBufferParameters.environmentLightParams.lightPresent;
    if (!params.initialSamplingParams.environmentMapImportanceSampling)
        params.initialSamplingParams.numPrimaryEnvironmentSamples = 0;
    params.temporalResamplingParams = restirDIContext.GetTemporalResamplingParameters();
    params.spatialResamplingParams = restirDIContext.GetSpatialResamplingParameters();
    params.shadingParams = restirDIContext.GetShadingParameters();
}

void FillReGIRConstants(ReGIR_Parameters& params, const rtxdi::ReGIRContext& regirContext)
{
    auto staticParams = regirContext.GetReGIRStaticParameters();
    auto dynamicParams = regirContext.GetReGIRDynamicParameters();
    auto gridParams = regirContext.GetReGIRGridCalculatedParameters();
    auto onionParams = regirContext.GetReGIROnionCalculatedParameters();

    params.gridParams.cellsX = staticParams.gridParameters.GridSize.x;
    params.gridParams.cellsY = staticParams.gridParameters.GridSize.y;
    params.gridParams.cellsZ = staticParams.gridParameters.GridSize.z;

    params.commonParams.numRegirBuildSamples = dynamicParams.regirNumBuildSamples;
    params.commonParams.risBufferOffset = regirContext.GetReGIRCellOffset();
    params.commonParams.lightsPerCell = staticParams.LightsPerCell;
    params.commonParams.centerX = dynamicParams.center.x;
    params.commonParams.centerY = dynamicParams.center.y;
    params.commonParams.centerZ = dynamicParams.center.z;
    params.commonParams.cellSize = (staticParams.Mode == rtxdi::ReGIRMode::Onion)
        ? dynamicParams.regirCellSize * 0.5f // Onion operates with radii, while "size" feels more like diameter
        : dynamicParams.regirCellSize;
    params.commonParams.localLightSamplingFallbackMode = static_cast<uint32_t>(dynamicParams.fallbackSamplingMode);
    params.commonParams.localLightPresamplingMode = static_cast<uint32_t>(dynamicParams.presamplingMode);
    params.commonParams.samplingJitter = std::max(0.f, dynamicParams.regirSamplingJitter * 2.f);
    params.onionParams.cubicRootFactor = onionParams.regirOnionCubicRootFactor;
    params.onionParams.linearFactor = onionParams.regirOnionLinearFactor;
    params.onionParams.numLayerGroups = uint32_t(onionParams.regirOnionLayers.size());

    assert(onionParams.regirOnionLayers.size() <= RTXDI_ONION_MAX_LAYER_GROUPS);
    for (int group = 0; group < int(onionParams.regirOnionLayers.size()); group++)
    {
        params.onionParams.layers[group] = onionParams.regirOnionLayers[group];
        params.onionParams.layers[group].innerRadius *= params.commonParams.cellSize;
        params.onionParams.layers[group].outerRadius *= params.commonParams.cellSize;
    }

    assert(onionParams.regirOnionRings.size() <= RTXDI_ONION_MAX_RINGS);
    for (int n = 0; n < int(onionParams.regirOnionRings.size()); n++)
    {
        params.onionParams.rings[n] = onionParams.regirOnionRings[n];
    }

    params.onionParams.cubicRootFactor = regirContext.GetReGIROnionCalculatedParameters().regirOnionCubicRootFactor;
}

void FillReSTIRGIConstants(ReSTIRGI_Parameters& constants, const rtxdi::ReSTIRGIContext& restirGIContext)
{
    constants.reservoirBufferParams = restirGIContext.GetReservoirBufferParameters();
    constants.bufferIndices = restirGIContext.GetBufferIndices();
    constants.temporalResamplingParams = restirGIContext.GetTemporalResamplingParameters();
    constants.spatialResamplingParams = restirGIContext.GetSpatialResamplingParameters();
    constants.finalShadingParams = restirGIContext.GetFinalShadingParameters();
}

void FillBRDFPTConstants(BRDFPathTracing_Parameters& constants, const GBufferSettings& gbufferSettings, const LightingPasses::RenderSettings& lightingSettings, const RTXDI_LightBufferParameters& lightBufferParameters)
{
    constants = lightingSettings.brdfptParams;
    constants.materialOverrideParams.minSecondaryRoughness = lightingSettings.brdfptParams.materialOverrideParams.minSecondaryRoughness;
    constants.materialOverrideParams.roughnessOverride = gbufferSettings.enableRoughnessOverride ? gbufferSettings.roughnessOverride : -1.f;
    constants.materialOverrideParams.metalnessOverride = gbufferSettings.enableMetalnessOverride ? gbufferSettings.metalnessOverride : -1.f;
    constants.secondarySurfaceReSTIRDIParams.initialSamplingParams.environmentMapImportanceSampling = lightBufferParameters.environmentLightParams.lightPresent;
    if (!constants.secondarySurfaceReSTIRDIParams.initialSamplingParams.environmentMapImportanceSampling)
        constants.secondarySurfaceReSTIRDIParams.initialSamplingParams.numPrimaryEnvironmentSamples = 0;
}

void LightingPasses::FillResamplingConstants(
    ResamplingConstants& constants,
    const RenderSettings& lightingSettings,
    const rtxdi::ImportanceSamplingContext& isContext)
{
    const RTXDI_LightBufferParameters& lightBufferParameters = isContext.GetLightBufferParameters();

    constants.enablePreviousTLAS = lightingSettings.enablePreviousTLAS;
    constants.denoiserMode = lightingSettings.denoiserMode;
    constants.sceneConstants.enableAlphaTestedGeometry = lightingSettings.enableAlphaTestedGeometry;
    constants.sceneConstants.enableTransparentGeometry = lightingSettings.enableTransparentGeometry;
    constants.visualizeRegirCells = lightingSettings.visualizeRegirCells;
#if WITH_NRD
    if (lightingSettings.denoiserMode != DENOISER_MODE_OFF)
    {
        NrdHitDistanceParamsToFloat4(lightingSettings.reblurDiffHitDistanceParams, constants.reblurDiffHitDistParams);
        NrdHitDistanceParamsToFloat4(lightingSettings.reblurSpecHitDistanceParams, constants.reblurSpecHitDistParams);
    }
#endif

    constants.lightBufferParams = isContext.GetLightBufferParameters();
    constants.localLightsRISBufferSegmentParams = isContext.GetLocalLightRISBufferSegmentParams();
    constants.environmentLightRISBufferSegmentParams = isContext.GetEnvironmentLightRISBufferSegmentParams();
    constants.runtimeParams = isContext.GetReSTIRDIContext().GetRuntimeParams();
    FillReSTIRDIConstants(constants.restirDI, isContext.GetReSTIRDIContext(), isContext.GetLightBufferParameters());
    FillReGIRConstants(constants.regir, isContext.GetReGIRContext());
    FillReSTIRGIConstants(constants.restirGI, isContext.GetReSTIRGIContext());

    constants.localLightPdfTextureSize = m_localLightPdfTextureSize;

    if (lightBufferParameters.environmentLightParams.lightPresent)
    {
        constants.environmentPdfTextureSize = m_environmentPdfTextureSize;
    }

    m_currentFrameOutputReservoir = isContext.GetReSTIRDIContext().GetBufferIndices().shadingInputBufferIndex;
}

void LightingPasses::PrepareForLightSampling(
    nvrhi::ICommandList* commandList,
    rtxdi::ImportanceSamplingContext& isContext,
    const donut::engine::IView& view,
    const donut::engine::IView& previousView,
    const RenderSettings& localSettings,
    bool enableAccumulation)
{
    rtxdi::ReSTIRDIContext& restirDIContext = isContext.GetReSTIRDIContext();
    rtxdi::ReGIRContext& regirContext = isContext.GetReGIRContext();

    ResamplingConstants constants = {};
    constants.frameIndex = restirDIContext.GetFrameIndex();
    view.FillPlanarViewConstants(constants.view);
    previousView.FillPlanarViewConstants(constants.prevView);
    FillResamplingConstants(constants, localSettings, isContext);
    constants.enableAccumulation = enableAccumulation;

    commandList->writeBuffer(m_constantBuffer, &constants, sizeof(constants));

    auto& lightBufferParams = isContext.GetLightBufferParameters();

    if (isContext.IsLocalLightPowerRISEnabled() &&
        lightBufferParams.localLightBufferRegion.numLights > 0)
    {
        dm::int2 presampleDispatchSize = {
            dm::div_ceil(isContext.GetLocalLightRISBufferSegmentParams().tileSize, RTXDI_PRESAMPLING_GROUP_SIZE),
            int(isContext.GetLocalLightRISBufferSegmentParams().tileCount)
        };

        ExecuteComputePass(commandList, m_presampleLightsPass, "PresampleLights", presampleDispatchSize, ProfilerSection::PresampleLights);
    }

    if (lightBufferParams.environmentLightParams.lightPresent)
    {
        dm::int2 presampleDispatchSize = {
            dm::div_ceil(isContext.GetEnvironmentLightRISBufferSegmentParams().tileSize, RTXDI_PRESAMPLING_GROUP_SIZE),
            int(isContext.GetEnvironmentLightRISBufferSegmentParams().tileCount)
        };

        ExecuteComputePass(commandList, m_presampleEnvironmentMapPass, "PresampleEnvironmentMap", presampleDispatchSize, ProfilerSection::PresampleEnvMap);
    }

    if (isContext.IsReGIREnabled() &&
        lightBufferParams.localLightBufferRegion.numLights > 0)
    {
        dm::int2 worldGridDispatchSize = {
            dm::div_ceil(regirContext.GetReGIRLightSlotCount(), RTXDI_GRID_BUILD_GROUP_SIZE),
            1
        };

        ExecuteComputePass(commandList, m_presampleReGIR, "PresampleReGIR", worldGridDispatchSize, ProfilerSection::PresampleReGIR);
    }
}

void LightingPasses::RenderDirectLighting(
    nvrhi::ICommandList* commandList,
    rtxdi::ReSTIRDIContext& context,
    const donut::engine::IView& view,
    const RenderSettings& localSettings)
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
        nvrhi::utils::BufferUavBarrier(commandList, m_lightReservoirBuffer);

        ExecuteRayTracingPass(commandList, m_fusedResamplingPass, localSettings.enableRayCounts, "DIFusedResampling", dispatchSize, ProfilerSection::Shading);
    }
    else
    {
        ExecuteRayTracingPass(commandList, m_generateInitialSamplesPass, localSettings.enableRayCounts, "DIGenerateInitialSamples", dispatchSize, ProfilerSection::InitialSamples);

        if (context.GetResamplingMode() == rtxdi::ReSTIRDI_ResamplingMode::Temporal || context.GetResamplingMode() == rtxdi::ReSTIRDI_ResamplingMode::TemporalAndSpatial)
        {
            nvrhi::utils::BufferUavBarrier(commandList, m_lightReservoirBuffer);

            ExecuteRayTracingPass(commandList, m_temporalResamplingPass, localSettings.enableRayCounts, "DITemporalResampling", dispatchSize, ProfilerSection::TemporalResampling);
        }

        if (context.GetResamplingMode() == rtxdi::ReSTIRDI_ResamplingMode::Spatial || context.GetResamplingMode() == rtxdi::ReSTIRDI_ResamplingMode::TemporalAndSpatial)
        {
            nvrhi::utils::BufferUavBarrier(commandList, m_lightReservoirBuffer);

            ExecuteRayTracingPass(commandList, m_spatialResamplingPass, localSettings.enableRayCounts, "DISpatialResampling", dispatchSize, ProfilerSection::SpatialResampling);
        }

        nvrhi::utils::BufferUavBarrier(commandList, m_lightReservoirBuffer);

        ExecuteRayTracingPass(commandList, m_shadeSamplesPass, localSettings.enableRayCounts, "DIShadeSamples", dispatchSize, ProfilerSection::Shading);
    }
    
    if (localSettings.enableGradients)
    {
        nvrhi::utils::BufferUavBarrier(commandList, m_lightReservoirBuffer);

        ExecuteRayTracingPass(commandList, m_gradientsPass, localSettings.enableRayCounts, "DIGradients", (dispatchSize + RTXDI_GRAD_FACTOR - 1) / RTXDI_GRAD_FACTOR, ProfilerSection::Gradients);
    }
}

void LightingPasses::RenderBrdfRays(
    nvrhi::ICommandList* commandList, 
    rtxdi::ImportanceSamplingContext& isContext,
    const donut::engine::IView& view,
    const donut::engine::IView& previousView,
    const RenderSettings& localSettings,
    const GBufferSettings& gbufferSettings,
    const EnvironmentLight& environmentLight,
    bool enableIndirect,
    bool enableAdditiveBlend,
    bool enableEmissiveSurfaces,
    bool enableAccumulation,
    bool enableReSTIRGI
    )
{
    ResamplingConstants constants = {};
    view.FillPlanarViewConstants(constants.view);
    previousView.FillPlanarViewConstants(constants.prevView);

    rtxdi::ReSTIRDIContext& restirDIContext = isContext.GetReSTIRDIContext();
    rtxdi::ReSTIRGIContext& restirGIContext = isContext.GetReSTIRGIContext();

    constants.frameIndex = restirDIContext.GetFrameIndex();
    constants.denoiserMode = localSettings.denoiserMode;
    constants.enableBrdfIndirect = enableIndirect;
    constants.enableBrdfAdditiveBlend = enableAdditiveBlend;
    constants.enableAccumulation = enableAccumulation;
    constants.sceneConstants.enableEnvironmentMap = (environmentLight.textureIndex >= 0);
    constants.sceneConstants.environmentMapTextureIndex = (environmentLight.textureIndex >= 0) ? environmentLight.textureIndex : 0;
    constants.sceneConstants.environmentScale = environmentLight.radianceScale.x;
    constants.sceneConstants.environmentRotation = environmentLight.rotation;
    FillResamplingConstants(constants, localSettings, isContext);
    FillBRDFPTConstants(constants.brdfPT, gbufferSettings, localSettings, isContext.GetLightBufferParameters());
    constants.brdfPT.enableIndirectEmissiveSurfaces = enableEmissiveSurfaces;
    constants.brdfPT.enableReSTIRGI = enableReSTIRGI;

    ReSTIRGI_BufferIndices restirGIBufferIndices = restirGIContext.GetBufferIndices();
    m_currentFrameGIOutputReservoir = restirGIBufferIndices.finalShadingInputBufferIndex;

    commandList->writeBuffer(m_constantBuffer, &constants, sizeof(constants));

    dm::int2 dispatchSize = {
        view.GetViewExtent().width(),
        view.GetViewExtent().height()
    };

    if (restirDIContext.GetStaticParameters().CheckerboardSamplingMode != rtxdi::CheckerboardMode::Off)
        dispatchSize.x /= 2;

    ExecuteRayTracingPass(commandList, m_brdfRayTracingPass, localSettings.enableRayCounts, "BrdfRayTracingPass", dispatchSize, ProfilerSection::BrdfRays);

    if (enableIndirect)
    {
        // Place an explicit UAV barrier between the passes. See the note on barriers in RenderDirectLighting(...)
        nvrhi::utils::BufferUavBarrier(commandList, m_secondarySurfaceBuffer);

        ExecuteRayTracingPass(commandList, m_shadeSecondarySurfacesPass, localSettings.enableRayCounts, "ShadeSecondarySurfaces", dispatchSize, ProfilerSection::ShadeSecondary, nullptr);
        
        if (enableReSTIRGI)
        {
            rtxdi::ReSTIRGI_ResamplingMode resamplingMode = restirGIContext.GetResamplingMode();
            if (resamplingMode == rtxdi::ReSTIRGI_ResamplingMode::FusedSpatiotemporal)
            {
                nvrhi::utils::BufferUavBarrier(commandList, m_GIReservoirBuffer);

                ExecuteRayTracingPass(commandList, m_GIFusedResamplingPass, localSettings.enableRayCounts, "GIFusedResampling", dispatchSize, ProfilerSection::GIFusedResampling, nullptr);
            }
            else
            {
                if (resamplingMode == rtxdi::ReSTIRGI_ResamplingMode::Temporal ||
                    resamplingMode == rtxdi::ReSTIRGI_ResamplingMode::TemporalAndSpatial)
                {
                    nvrhi::utils::BufferUavBarrier(commandList, m_GIReservoirBuffer);

                    ExecuteRayTracingPass(commandList, m_GITemporalResamplingPass, localSettings.enableRayCounts, "GITemporalResampling", dispatchSize, ProfilerSection::GITemporalResampling, nullptr);
                }

                if (resamplingMode == rtxdi::ReSTIRGI_ResamplingMode::Spatial ||
                    resamplingMode == rtxdi::ReSTIRGI_ResamplingMode::TemporalAndSpatial)
                {
                    nvrhi::utils::BufferUavBarrier(commandList, m_GIReservoirBuffer);

                    ExecuteRayTracingPass(commandList, m_GISpatialResamplingPass, localSettings.enableRayCounts, "GISpatialResampling", dispatchSize, ProfilerSection::GISpatialResampling, nullptr);
                }
            }

            nvrhi::utils::BufferUavBarrier(commandList, m_GIReservoirBuffer);

            ExecuteRayTracingPass(commandList, m_GIFinalShadingPass, localSettings.enableRayCounts, "GIFinalShading", dispatchSize, ProfilerSection::GIFinalShading, nullptr);
        }
    }
}

void LightingPasses::NextFrame()
{
    std::swap(m_bindingSet, m_prevBindingSet);
    m_lastFrameOutputReservoir = m_currentFrameOutputReservoir;
}

nvrhi::IBindingLayout* LightingPasses::GetBindingLayout() const
{
    return m_bindingLayout;
}

nvrhi::IBindingSet* LightingPasses::GetCurrentBindingSet() const
{
    return m_bindingSet;
}

uint32_t LightingPasses::GetOutputReservoirBufferIndex() const
{
    return m_currentFrameOutputReservoir;
}

uint32_t LightingPasses::GetGIOutputReservoirBufferIndex() const
{
    return m_currentFrameGIOutputReservoir;
}