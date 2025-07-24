/***************************************************************************
 # Copyright (c) 2020-2023, NVIDIA CORPORATION.  All rights reserved.
 #
 # NVIDIA CORPORATION and its licensors retain all intellectual property
 # and proprietary rights in and to this software, related documentation
 # and any modifications thereto.  Any use, reproduction, disclosure or
 # distribution of this software and related documentation without an express
 # license agreement from NVIDIA CORPORATION is strictly prohibited.
 **************************************************************************/

// Include this first just to test the cleanliness
#include <Rtxdi/ImportanceSamplingContext.h>

#include <donut/render/ToneMappingPasses.h>
#include <donut/render/TemporalAntiAliasingPass.h>
#include <donut/app/ApplicationBase.h>
#include <donut/app/Camera.h>
#include <donut/engine/ShaderFactory.h>
#include <donut/engine/CommonRenderPasses.h>
#include <donut/engine/TextureCache.h>
#include <donut/render/BloomPass.h>
#include <donut/render/SkyPass.h>
#include <donut/engine/Scene.h>
#include <donut/engine/FramebufferFactory.h>
#include <donut/engine/DescriptorTableManager.h>
#include <donut/engine/View.h>
#include <donut/engine/IesProfile.h>
#include <donut/app/DeviceManager.h>
#include <donut/core/log.h>
#include <donut/core/vfs/VFS.h>
#include <donut/core/math/math.h>
#include <nvrhi/utils.h>
#ifdef DONUT_WITH_TASKFLOW
#include <taskflow/taskflow.hpp>
#endif

#include "DebugViz/DebugVizPasses.h"
#include "RenderPasses/AccumulationPass.h"
#include "RenderPasses/CompositingPass.h"
#include "RenderPasses/ConfidencePass.h"
#include "RenderPasses/FilterGradientsPass.h"
#include "RenderPasses/GBufferPass.h"
#include "RenderPasses/GenerateMipsPass.h"
#include "RenderPasses/GlassPass.h"
#include "RenderPasses/LightingPasses.h"
#include "RenderPasses/PrepareLightsPass.h"
#include "RenderPasses/RenderEnvironmentMapPass.h"
#include "RenderPasses/VisualizationPass.h"
#include "Profiler.h"
#include "RenderTargets.h"
#include "RtxdiResources.h"
#include "SampleScene.h"
#include "Testing.h"
#include "UserInterface.h"

#if WITH_NRD
#include "NrdIntegration.h"
#endif

#if WITH_DLSS
#include "DLSS.h"
#endif

#ifndef _WIN32
#include <unistd.h>
#else
extern "C" {
  // Prefer using the discrete GPU on Optimus laptops
  _declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
}
#endif

using namespace donut;
using namespace donut::math;
using namespace std::chrono;
#include "../shaders/ShaderParameters.h"

static int g_ExitCode = 0;

class SceneRenderer : public app::ApplicationBase
{
public:
    SceneRenderer(app::DeviceManager* deviceManager, UIData& ui, CommandLineArguments& args)
        : ApplicationBase(deviceManager)
        , m_bindingCache(deviceManager->GetDevice())
        , m_ui(ui)
        , m_args(args)
    { 
        m_ui.resources->camera = &m_camera;
    }

    [[nodiscard]] std::shared_ptr<engine::ShaderFactory> GetShaderFactory() const
    {
        return m_shaderFactory;
    }

    [[nodiscard]] std::shared_ptr<vfs::IFileSystem> GetRootFs() const
    {
        return m_rootFs;
    }

    bool Init()
    {
        std::filesystem::path mediaPath = app::GetDirectoryWithExecutable().parent_path() / "Assets/Media";
        if (!std::filesystem::exists(mediaPath))
        {
            mediaPath = app::GetDirectoryWithExecutable().parent_path().parent_path() / "Assets/Media";
            if (!std::filesystem::exists(mediaPath))
            {
                log::error("Couldn't locate the 'Assets/Media' folder.");
                return false;
            }
        }

        std::filesystem::path frameworkShaderPath = app::GetDirectoryWithExecutable() / "shaders/framework" / app::GetShaderTypeName(GetDevice()->getGraphicsAPI());
        std::filesystem::path appShaderPath = app::GetDirectoryWithExecutable() / "shaders/full-sample" / app::GetShaderTypeName(GetDevice()->getGraphicsAPI());

        log::debug("Mounting %s to %s", mediaPath.string().c_str(), "/Assets/Media");
        log::debug("Mounting %s to %s", frameworkShaderPath.string().c_str(), "/shaders/donut");
        log::debug("Mounting %s to %s", appShaderPath.string().c_str(), "/shaders/app");

        m_rootFs = std::make_shared<vfs::RootFileSystem>();
        m_rootFs->mount("/Assets/Media", mediaPath);
        m_rootFs->mount("/shaders/donut", frameworkShaderPath);
        m_rootFs->mount("/shaders/app", appShaderPath);

        m_shaderFactory = std::make_shared<engine::ShaderFactory>(GetDevice(), m_rootFs, "/shaders");
        m_CommonPasses = std::make_shared<engine::CommonRenderPasses>(GetDevice(), m_shaderFactory);

        {
            nvrhi::BindlessLayoutDesc bindlessLayoutDesc;
            bindlessLayoutDesc.firstSlot = 0;
            bindlessLayoutDesc.registerSpaces = {
                nvrhi::BindingLayoutItem::RawBuffer_SRV(1),
                nvrhi::BindingLayoutItem::Texture_SRV(2),
                nvrhi::BindingLayoutItem::Texture_UAV(3)
            };
            bindlessLayoutDesc.visibility = nvrhi::ShaderType::All;
            bindlessLayoutDesc.maxCapacity = 1024;
            m_bindlessLayout = GetDevice()->createBindlessLayout(bindlessLayoutDesc);
        }

        std::filesystem::path scenePath = "/Assets/Media/bistro-rtxdi.scene.json";

        m_descriptorTableManager = std::make_shared<engine::DescriptorTableManager>(GetDevice(), m_bindlessLayout);

        m_TextureCache = std::make_shared<donut::engine::TextureCache>(GetDevice(), m_rootFs, m_descriptorTableManager);
        m_TextureCache->SetInfoLogSeverity(donut::log::Severity::Debug);
        
        m_iesProfileLoader = std::make_unique<engine::IesProfileLoader>(GetDevice(), m_shaderFactory, m_descriptorTableManager);

        auto sceneTypeFactory = std::make_shared<SampleSceneTypeFactory>();
        m_scene = std::make_shared<SampleScene>(GetDevice(), *m_shaderFactory, m_rootFs, m_TextureCache, m_descriptorTableManager, sceneTypeFactory);
        m_ui.resources->scene = m_scene;

        SetAsynchronousLoadingEnabled(true);
        BeginLoadingScene(m_rootFs, scenePath);
        GetDeviceManager()->SetVsyncEnabled(true);

        if (!GetDevice()->queryFeatureSupport(nvrhi::Feature::RayQuery))
            m_ui.useRayQuery = false;

        m_profiler = std::make_shared<Profiler>(*GetDeviceManager());
        m_ui.resources->profiler = m_profiler;

        m_filterGradientsPass = std::make_unique<FilterGradientsPass>(GetDevice(), m_shaderFactory);
        m_confidencePass = std::make_unique<ConfidencePass>(GetDevice(), m_shaderFactory);
        m_compositingPass = std::make_unique<CompositingPass>(GetDevice(), m_shaderFactory, m_CommonPasses, m_scene, m_bindlessLayout);
        m_accumulationPass = std::make_unique<AccumulationPass>(GetDevice(), m_shaderFactory);
        m_gBufferPass = std::make_unique<RaytracedGBufferPass>(GetDevice(), m_shaderFactory, m_CommonPasses, m_scene, m_profiler, m_bindlessLayout);
        m_rasterizedGBufferPass = std::make_unique<RasterizedGBufferPass>(GetDevice(), m_shaderFactory, m_CommonPasses, m_scene, m_profiler, m_bindlessLayout);
        m_postprocessGBufferPass = std::make_unique<PostprocessGBufferPass>(GetDevice(), m_shaderFactory);
        m_glassPass = std::make_unique<GlassPass>(GetDevice(), m_shaderFactory, m_CommonPasses, m_scene, m_profiler, m_bindlessLayout);
        m_prepareLightsPass = std::make_unique<PrepareLightsPass>(GetDevice(), m_shaderFactory, m_CommonPasses, m_scene, m_bindlessLayout);
        m_lightingPasses = std::make_unique<LightingPasses>(GetDevice(), m_shaderFactory, m_CommonPasses, m_scene, m_profiler, m_bindlessLayout);


#if WITH_DLSS
        {
#if DONUT_WITH_DX12
            if (GetDevice()->getGraphicsAPI() == nvrhi::GraphicsAPI::D3D12)
                m_dlss = DLSS::CreateDX12(GetDevice(), *m_shaderFactory);
#endif
#if DONUT_WITH_VULKAN
            if (GetDevice()->getGraphicsAPI() == nvrhi::GraphicsAPI::VULKAN)
                m_dlss = DLSS::CreateVK(GetDevice(), *m_shaderFactory);
#endif
        }
#endif

        LoadShaders();

        std::vector<std::string> profileNames;
        m_rootFs->enumerateFiles("/Assets/Media/ies-profiles", { ".ies" }, vfs::enumerate_to_vector(profileNames));

        for (const std::string& profileName : profileNames)
        {
            auto profile = m_iesProfileLoader->LoadIesProfile(*m_rootFs, "/Assets/Media/ies-profiles/" + profileName);

            if (profile)
            {
                m_iesProfiles.push_back(profile);
            }
        }
        m_ui.resources->iesProfiles = m_iesProfiles;

        m_commandList = GetDevice()->createCommandList();

        return true;
    }

    void AssignIesProfiles(nvrhi::ICommandList* commandList)
    {
        for (const auto& light : m_scene->GetSceneGraph()->GetLights())
        {
            if (light->GetLightType() == LightType_Spot)
            {
                SpotLightWithProfile& spotLight = static_cast<SpotLightWithProfile&>(*light);

                if (spotLight.profileName.empty())
                    continue;

                if (spotLight.profileTextureIndex >= 0)
                    continue;

                auto foundProfile = std::find_if(m_iesProfiles.begin(), m_iesProfiles.end(),
                    [&spotLight](auto it) { return it->name == spotLight.profileName; });

                if (foundProfile != m_iesProfiles.end())
                {
                    m_iesProfileLoader->BakeIesProfile(**foundProfile, commandList);

                    spotLight.profileTextureIndex = (*foundProfile)->textureIndex;
                }
            }
        }
    }

    virtual void SceneLoaded() override
    {
        ApplicationBase::SceneLoaded();

        m_scene->FinishedLoading(GetFrameIndex());

        m_camera.LookAt(float3(-7.688f, 2.0f, 5.594f), float3(-7.3341f, 2.0f, 6.5366f));
        m_camera.SetMoveSpeed(3.f);

        const auto& sceneGraph = m_scene->GetSceneGraph();

        for (const auto& pLight : sceneGraph->GetLights())
        {
            if (pLight->GetLightType() == LightType_Directional)
            {
                m_sunLight = std::static_pointer_cast<engine::DirectionalLight>(pLight);
                break;
            }
        }

        if (!m_sunLight)
        {
            m_sunLight = std::make_shared<engine::DirectionalLight>();
            sceneGraph->AttachLeafNode(sceneGraph->GetRootNode(), m_sunLight);
            m_sunLight->SetDirection(dm::double3(0.15, -1.0, 0.3));
            m_sunLight->angularSize = 1.f;
        }

        m_commandList->open();
        AssignIesProfiles(m_commandList);
        m_commandList->close();
        GetDevice()->executeCommandList(m_commandList);

        // Create an environment light
        m_environmentLight = std::make_shared<EnvironmentLight>();
        sceneGraph->AttachLeafNode(sceneGraph->GetRootNode(), m_environmentLight);
        m_environmentLight->SetName("Environment");
        m_ui.environmentMapDirty = 2;
        m_ui.environmentMapIndex = 0;
        
        m_rasterizedGBufferPass->CreateBindingSet();

        m_scene->BuildMeshBLASes(GetDevice());

        GetDeviceManager()->SetVsyncEnabled(false);

        m_ui.isLoading = false;
    }
    
    void LoadShaders()
    {
        m_filterGradientsPass->CreatePipeline();
        m_confidencePass->CreatePipeline();
        m_compositingPass->CreatePipeline();
        m_accumulationPass->CreatePipeline();
        m_gBufferPass->CreatePipeline(m_ui.useRayQuery);
        m_postprocessGBufferPass->CreatePipeline();
        m_glassPass->CreatePipeline(m_ui.useRayQuery);
        m_prepareLightsPass->CreatePipeline();
    }

    virtual bool LoadScene(std::shared_ptr<vfs::IFileSystem> fs, const std::filesystem::path& sceneFileName) override 
    {
        if (m_scene->Load(sceneFileName))
        {
            return true;
        }

        return false;
    }

    bool KeyboardUpdate(int key, int scancode, int action, int mods) override
    {
        if (key == GLFW_KEY_GRAVE_ACCENT && action == GLFW_PRESS)
        {
            m_ui.showUI = !m_ui.showUI;
            return true;
        }

        if (mods == GLFW_MOD_CONTROL && key == GLFW_KEY_R && action == GLFW_PRESS)
        {
            m_ui.reloadShaders = true;
            return true;
        }

        if (mods == 0 && key == GLFW_KEY_F1 && action == GLFW_PRESS)
        {
            m_frameStepMode = (m_frameStepMode == FrameStepMode::Disabled) ? FrameStepMode::Wait : FrameStepMode::Disabled;
            return true;
        }

        if (mods == 0 && key == GLFW_KEY_F2 && action == GLFW_PRESS)
        {
            if (m_frameStepMode == FrameStepMode::Wait)
                m_frameStepMode = FrameStepMode::Step;
            return true;
        }

        if (mods == 0 && key == GLFW_KEY_F5 && action == GLFW_PRESS)
        {
            if (m_ui.animationFrame.has_value())
            {
                // Stop benchmark if it's running
                m_ui.animationFrame.reset();
            }
            else
            {
                // Start benchmark otherwise
                m_ui.animationFrame = std::optional<int>(0);
            }
            return true;
        }
        
        m_camera.KeyboardUpdate(key, scancode, action, mods);

        return true;
    }

    virtual bool MousePosUpdate(double xpos, double ypos) override
    {
        m_camera.MousePosUpdate(xpos, ypos);
        return true;
    }

    virtual bool MouseButtonUpdate(int button, int action, int mods) override
    {
        if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
        {
            double mousex = 0, mousey = 0;
            glfwGetCursorPos(GetDeviceManager()->GetWindow(), &mousex, &mousey);

            // Scale the mouse position according to the render resolution scale
            mousex *= m_view.GetViewport().width() / m_upscaledView.GetViewport().width();
            mousey *= m_view.GetViewport().height() / m_upscaledView.GetViewport().height();

            m_ui.gbufferSettings.materialReadbackPosition = int2(int(mousex), int(mousey));
            m_ui.gbufferSettings.enableMaterialReadback = true;
            return true;
        }

        m_camera.MouseButtonUpdate(button, action, mods);
        return true;
    }

    virtual void Animate(float fElapsedTimeSeconds) override
    {
        if (m_ui.isLoading)
            return;

        if (!m_args.saveFrameFileName.empty())
            fElapsedTimeSeconds = 1.f / 60.f;

        m_camera.Animate(fElapsedTimeSeconds);

        if (m_ui.enableAnimations)
            m_scene->Animate(fElapsedTimeSeconds * m_ui.animationSpeed);

        if (m_toneMappingPass)
            m_toneMappingPass->AdvanceFrame(fElapsedTimeSeconds);
    }

    virtual void BackBufferResized(const uint32_t width, const uint32_t height, const uint32_t sampleCount) override
    {
        // If the render size is overridden from the command line, ignore the window size.
        if (m_args.renderWidth > 0 && m_args.renderHeight > 0)
            return;

        if (m_renderTargets && m_renderTargets->Size.x == int(width) && m_renderTargets->Size.y == int(height))
            return;

        m_bindingCache.Clear();
        m_renderTargets = nullptr;
        m_isContext = nullptr;
        m_rtxdiResources = nullptr;
        m_temporalAntiAliasingPass = nullptr;
        m_toneMappingPass = nullptr;
        m_bloomPass = nullptr;
#if WITH_NRD
        m_nrd = nullptr;
#endif
    }

    void LoadEnvironmentMap()
    {
        if (m_environmentMap)
        {
            // Make sure there is no rendering in-flight before we unload the texture and erase its descriptor.
            // Decsriptor manipulations are synchronous and immediately affect whatever is executing on the GPU.
            GetDevice()->waitForIdle();

            m_TextureCache->UnloadTexture(m_environmentMap);
            
            m_environmentMap = nullptr;
        }

        if (m_ui.environmentMapIndex > 0)
        {
            auto& environmentMaps = m_scene->GetEnvironmentMaps();
            const std::string& environmentMapPath = environmentMaps[m_ui.environmentMapIndex];

            m_environmentMap = m_TextureCache->LoadTextureFromFileDeferred(environmentMapPath, false);

            if (m_TextureCache->IsTextureLoaded(m_environmentMap))
            {
                m_TextureCache->ProcessRenderingThreadCommands(*m_CommonPasses, 0.f);
                m_TextureCache->LoadingFinished();

                m_environmentMap->bindlessDescriptor = m_descriptorTableManager->CreateDescriptorHandle(nvrhi::BindingSetItem::Texture_SRV(0, m_environmentMap->texture));
            }
            else
            {
                // Failed to load the file: revert to the procedural map and remove this file from the list.
                m_environmentMap = nullptr;
                environmentMaps.erase(environmentMaps.begin() + m_ui.environmentMapIndex);
                m_ui.environmentMapIndex = 0;
            }
        }
    }

    void SetupView(uint32_t renderWidth, uint32_t renderHeight, const engine::PerspectiveCamera* activeCamera)
    {
        nvrhi::Viewport windowViewport((float)renderWidth, (float)renderHeight);

        if (m_temporalAntiAliasingPass)
            m_temporalAntiAliasingPass->SetJitter(m_ui.temporalJitter);

        nvrhi::Viewport renderViewport = windowViewport;
        renderViewport.maxX = roundf(renderViewport.maxX * m_ui.resolutionScale);
        renderViewport.maxY = roundf(renderViewport.maxY * m_ui.resolutionScale);

        m_view.SetViewport(renderViewport);

        if (m_ui.enablePixelJitter && m_temporalAntiAliasingPass)
        {
            m_view.SetPixelOffset(m_temporalAntiAliasingPass->GetCurrentPixelOffset());
        }
        else
        {
            m_view.SetPixelOffset(0.f);
        }

        const float aspectRatio = windowViewport.width() / windowViewport.height();
        if (activeCamera)
            m_view.SetMatrices(activeCamera->GetWorldToViewMatrix(), perspProjD3DStyleReverse(activeCamera->verticalFov, aspectRatio, activeCamera->zNear));
        else
            m_view.SetMatrices(m_camera.GetWorldToViewMatrix(), perspProjD3DStyleReverse(radians(m_ui.verticalFov), aspectRatio, 0.01f));
        m_view.UpdateCache();

        if (m_viewPrevious.GetViewExtent().width() == 0)
            m_viewPrevious = m_view;

        m_upscaledView = m_view;
        m_upscaledView.SetViewport(windowViewport);
    }

    void SetupRenderPasses(uint32_t renderWidth, uint32_t renderHeight, bool& exposureResetRequired)
    {
        if (m_ui.environmentMapDirty == 2)
        {
            m_environmentMapPdfMipmapPass = nullptr;

            m_ui.environmentMapDirty = 1;
        }

        if (m_ui.reloadShaders)
        {
            GetDevice()->waitForIdle();

            m_shaderFactory->ClearCache();
            m_temporalAntiAliasingPass = nullptr;
            m_renderEnvironmentMapPass = nullptr;
            m_environmentMapPdfMipmapPass = nullptr;
            m_localLightPdfMipmapPass = nullptr;
            m_visualizationPass = nullptr;
            m_debugVizPasses = nullptr;
            m_ui.environmentMapDirty = 1;

            LoadShaders();
        }

        bool renderTargetsCreated = false;
        bool rtxdiResourcesCreated = false;

        if (!m_renderEnvironmentMapPass)
        {
            m_renderEnvironmentMapPass = std::make_unique<RenderEnvironmentMapPass>(GetDevice(), m_shaderFactory, m_descriptorTableManager, 2048);
        }
        
        const auto environmentMap = (m_ui.environmentMapIndex > 0)
            ? m_environmentMap->texture.Get()
            : m_renderEnvironmentMapPass->GetTexture();

        uint32_t numEmissiveMeshes, numEmissiveTriangles;
        m_prepareLightsPass->CountLightsInScene(numEmissiveMeshes, numEmissiveTriangles);
        uint32_t numPrimitiveLights = uint32_t(m_scene->GetSceneGraph()->GetLights().size());
        uint32_t numGeometryInstances = uint32_t(m_scene->GetSceneGraph()->GetGeometryInstancesCount());
        
        uint2 environmentMapSize = uint2(environmentMap->getDesc().width, environmentMap->getDesc().height);

        if (m_rtxdiResources && (
            environmentMapSize.x != m_rtxdiResources->EnvironmentPdfTexture->getDesc().width ||
            environmentMapSize.y != m_rtxdiResources->EnvironmentPdfTexture->getDesc().height ||
            numEmissiveMeshes > m_rtxdiResources->GetMaxEmissiveMeshes() ||
            numEmissiveTriangles > m_rtxdiResources->GetMaxEmissiveTriangles() || 
            numPrimitiveLights > m_rtxdiResources->GetMaxPrimitiveLights() ||
            numGeometryInstances > m_rtxdiResources->GetMaxGeometryInstances()))
        {
            m_rtxdiResources = nullptr;
        }

        if (!m_isContext)
        {
            rtxdi::ImportanceSamplingContext_StaticParameters isStaticParams;
            isStaticParams.CheckerboardSamplingMode = m_ui.restirDIStaticParams.CheckerboardSamplingMode;
            isStaticParams.renderHeight = renderHeight;
            isStaticParams.renderWidth = renderWidth;
            isStaticParams.regirStaticParams = m_ui.regirStaticParams;

            m_isContext = std::make_unique<rtxdi::ImportanceSamplingContext>(isStaticParams);

            m_ui.regirLightSlotCount = m_isContext->GetReGIRContext().GetReGIRLightSlotCount();
        }

        if (!m_renderTargets)
        {
            m_renderTargets = std::make_shared<RenderTargets>(GetDevice(), int2((int)renderWidth, (int)renderHeight));

            m_profiler->SetRenderTargets(m_renderTargets);

            m_gBufferPass->CreateBindingSet(m_scene->GetTopLevelAS(), m_scene->GetPrevTopLevelAS(), *m_renderTargets);

            m_postprocessGBufferPass->CreateBindingSet(*m_renderTargets);

            m_glassPass->CreateBindingSet(m_scene->GetTopLevelAS(), m_scene->GetPrevTopLevelAS(), *m_renderTargets);

            m_filterGradientsPass->CreateBindingSet(*m_renderTargets);

            m_confidencePass->CreateBindingSet(*m_renderTargets);
            
            m_accumulationPass->CreateBindingSet(*m_renderTargets);

            m_rasterizedGBufferPass->CreatePipeline(*m_renderTargets);

            m_compositingPass->CreateBindingSet(*m_renderTargets);

            m_visualizationPass = nullptr;
            m_debugVizPasses = nullptr;

            renderTargetsCreated = true;
        }

        if (!m_rtxdiResources)
        {
            uint32_t meshAllocationQuantum = 128;
            uint32_t triangleAllocationQuantum = 1024;
            uint32_t primitiveAllocationQuantum = 128;

            m_rtxdiResources = std::make_unique<RtxdiResources>(
                GetDevice(), 
                m_isContext->GetReSTIRDIContext(),
                m_isContext->GetRISBufferSegmentAllocator(),
                (numEmissiveMeshes + meshAllocationQuantum - 1) & ~(meshAllocationQuantum - 1),
                (numEmissiveTriangles + triangleAllocationQuantum - 1) & ~(triangleAllocationQuantum - 1),
                (numPrimitiveLights + primitiveAllocationQuantum - 1) & ~(primitiveAllocationQuantum - 1),
                numGeometryInstances,
                environmentMapSize.x,
                environmentMapSize.y);

            m_prepareLightsPass->CreateBindingSet(*m_rtxdiResources);
            
            rtxdiResourcesCreated = true;

            // Make sure that the environment PDF map is re-generated
            m_ui.environmentMapDirty = 1;
        }
        
        if (!m_environmentMapPdfMipmapPass || rtxdiResourcesCreated)
        {
            m_environmentMapPdfMipmapPass = std::make_unique<GenerateMipsPass>(
                GetDevice(),
                m_shaderFactory,
                environmentMap,
                m_rtxdiResources->EnvironmentPdfTexture);
        }

        if (!m_localLightPdfMipmapPass || rtxdiResourcesCreated)
        {
            m_localLightPdfMipmapPass = std::make_unique<GenerateMipsPass>(
                GetDevice(),
                m_shaderFactory,
                nullptr,
                m_rtxdiResources->LocalLightPdfTexture);
        }

        if (renderTargetsCreated || rtxdiResourcesCreated)
        {
            m_lightingPasses->CreateBindingSet(
                m_scene->GetTopLevelAS(),
                m_scene->GetPrevTopLevelAS(),
                *m_renderTargets,
                *m_rtxdiResources);
        }

        if (rtxdiResourcesCreated || m_ui.reloadShaders)
        {
            // Some RTXDI context settings affect the shader permutations
            m_lightingPasses->CreatePipelines(m_ui.regirStaticParams, m_ui.useRayQuery);
        }

        m_ui.reloadShaders = false;

        if (!m_temporalAntiAliasingPass)
        {
            render::TemporalAntiAliasingPass::CreateParameters taaParams;
            taaParams.motionVectors = m_renderTargets->MotionVectors;
            taaParams.unresolvedColor = m_renderTargets->HdrColor;
            taaParams.resolvedColor = m_renderTargets->ResolvedColor;
            taaParams.feedback1 = m_renderTargets->TaaFeedback1;
            taaParams.feedback2 = m_renderTargets->TaaFeedback2;
            taaParams.useCatmullRomFilter = true;

            m_temporalAntiAliasingPass = std::make_unique<render::TemporalAntiAliasingPass>(
                GetDevice(), m_shaderFactory, m_CommonPasses, m_view, taaParams);
        }

        exposureResetRequired = false;
        if (!m_toneMappingPass)
        {
            render::ToneMappingPass::CreateParameters toneMappingParams;
            m_toneMappingPass = std::make_unique<render::ToneMappingPass>(GetDevice(), m_shaderFactory, m_CommonPasses, m_renderTargets->LdrFramebuffer, m_upscaledView, toneMappingParams);
            exposureResetRequired = true;
        }

        if (!m_bloomPass)
        {
            m_bloomPass = std::make_unique<render::BloomPass>(GetDevice(), m_shaderFactory, m_CommonPasses, m_renderTargets->ResolvedFramebuffer, m_upscaledView);
        }

        if (!m_visualizationPass || renderTargetsCreated || rtxdiResourcesCreated)
        {
            m_visualizationPass = std::make_unique<VisualizationPass>(GetDevice(), *m_CommonPasses, *m_shaderFactory, *m_renderTargets, *m_rtxdiResources);
        }

        if (!m_debugVizPasses || renderTargetsCreated)
        {
            m_debugVizPasses = std::make_unique<DebugVizPasses>(GetDevice(), m_shaderFactory, m_scene, m_bindlessLayout);
            m_debugVizPasses->CreateBindingSets(*m_renderTargets, m_renderTargets->DebugColor);
            m_debugVizPasses->CreatePipelines();
        }

#if WITH_NRD
        if (!m_nrd)
        {
            m_nrd = std::make_unique<NrdIntegration>(GetDevice(), m_ui.denoisingMethod);
            m_nrd->Initialize(m_renderTargets->Size.x, m_renderTargets->Size.y);
        }
#endif
#if WITH_DLSS
        {
            m_dlss->SetRenderSize(m_renderTargets->Size.x, m_renderTargets->Size.y, m_renderTargets->Size.x, m_renderTargets->Size.y);
            
            m_ui.dlssAvailable = m_dlss->IsAvailable();
        }
#endif
    }

    virtual void RenderSplashScreen(nvrhi::IFramebuffer* framebuffer) override
    {
        nvrhi::ITexture* framebufferTexture = framebuffer->getDesc().colorAttachments[0].texture;
        m_commandList->open();
        m_commandList->clearTextureFloat(framebufferTexture, nvrhi::AllSubresources, nvrhi::Color(0.f));
        m_commandList->close();
        GetDevice()->executeCommandList(m_commandList);

        uint32_t loadedObjects = engine::Scene::GetLoadingStats().ObjectsLoaded;
        uint32_t requestedObjects = engine::Scene::GetLoadingStats().ObjectsTotal;
        uint32_t loadedTextures = m_TextureCache->GetNumberOfLoadedTextures();
        uint32_t finalizedTextures = m_TextureCache->GetNumberOfFinalizedTextures();
        uint32_t requestedTextures = m_TextureCache->GetNumberOfRequestedTextures();
        uint32_t objectMultiplier = 20;
        m_ui.loadingPercentage = (requestedTextures > 0) 
            ? float(loadedTextures + finalizedTextures + loadedObjects * objectMultiplier) / float(requestedTextures * 2 + requestedObjects * objectMultiplier) 
            : 0.f;
    }

    void Resolve(nvrhi::ICommandList* commandList, float accumulationWeight) const
    {
        ProfilerScope scope(*m_profiler, commandList, ProfilerSection::Resolve);

        switch (m_ui.aaMode)
        {
        case AntiAliasingMode::None: {
            engine::BlitParameters blitParams;
            blitParams.sourceTexture = m_renderTargets->HdrColor;
            blitParams.sourceBox.m_maxs.x = m_view.GetViewport().width() / m_upscaledView.GetViewport().width();
            blitParams.sourceBox.m_maxs.y = m_view.GetViewport().height() / m_upscaledView.GetViewport().height();
            blitParams.targetFramebuffer = m_renderTargets->ResolvedFramebuffer->GetFramebuffer(m_upscaledView);
            m_CommonPasses->BlitTexture(commandList, blitParams);
            break;
        }

        case AntiAliasingMode::Accumulation: {
            m_accumulationPass->Render(commandList, m_view, m_upscaledView, accumulationWeight);
            m_CommonPasses->BlitTexture(commandList, m_renderTargets->ResolvedFramebuffer->GetFramebuffer(m_upscaledView), m_renderTargets->AccumulatedColor);
            break;
        }

        case AntiAliasingMode::TAA: {
            auto taaParams = m_ui.taaParams;
            if (m_ui.resetAccumulation)
                taaParams.newFrameWeight = 1.f;

            m_temporalAntiAliasingPass->TemporalResolve(commandList, taaParams, m_previousViewValid, m_view, m_upscaledView);
            break;
        }

#if WITH_DLSS
        case AntiAliasingMode::DLSS: {
            m_dlss->Render(commandList, *m_renderTargets, m_toneMappingPass->GetExposureBuffer(), m_ui.dlssExposureScale, m_ui.dlssSharpness, m_ui.rasterizeGBuffer, m_ui.resetAccumulation, m_view, m_viewPrevious);
            break;
        }
#endif
        }
    }

    void UpdateReGIRContextFromUI()
    {
        auto& regirContext = m_isContext->GetReGIRContext();
        auto dynamicParams = m_ui.regirDynamicParameters;
        dynamicParams.center = { m_regirCenter.x, m_regirCenter.y, m_regirCenter.z };
        regirContext.SetDynamicParameters(dynamicParams);
    }

    void UpdateReSTIRDIContextFromUI()
    {
        rtxdi::ReSTIRDIContext& restirDIContext = m_isContext->GetReSTIRDIContext();
        ReSTIRDI_InitialSamplingParameters initialSamplingParams = m_ui.restirDI.initialSamplingParams;
        switch (initialSamplingParams.localLightSamplingMode)
        {
        default:
        case ReSTIRDI_LocalLightSamplingMode::Uniform:
            initialSamplingParams.numPrimaryLocalLightSamples = m_ui.restirDI.numLocalLightUniformSamples;
            break;
        case ReSTIRDI_LocalLightSamplingMode::Power_RIS:
            initialSamplingParams.numPrimaryLocalLightSamples = m_ui.restirDI.numLocalLightPowerRISSamples;
            break;
        case ReSTIRDI_LocalLightSamplingMode::ReGIR_RIS:
            initialSamplingParams.numPrimaryLocalLightSamples = m_ui.restirDI.numLocalLightReGIRRISSamples;
            break;
        }
        restirDIContext.SetResamplingMode(m_ui.restirDI.resamplingMode);
        restirDIContext.SetInitialSamplingParameters(initialSamplingParams);
        restirDIContext.SetTemporalResamplingParameters(m_ui.restirDI.temporalResamplingParams);
        restirDIContext.SetSpatialResamplingParameters(m_ui.restirDI.spatialResamplingParams);
        restirDIContext.SetShadingParameters(m_ui.restirDI.shadingParams);
    }

    void UpdateReSTIRGIContextFromUI()
    {
        rtxdi::ReSTIRGIContext& restirGIContext = m_isContext->GetReSTIRGIContext();
        restirGIContext.SetResamplingMode(m_ui.restirGI.resamplingMode);
        restirGIContext.SetTemporalResamplingParameters(m_ui.restirGI.temporalResamplingParams);
        restirGIContext.SetSpatialResamplingParameters(m_ui.restirGI.spatialResamplingParams);
        restirGIContext.SetFinalShadingParameters(m_ui.restirGI.finalShadingParams);
    }

    bool IsLocalLightPowerRISEnabled()
    {
        if (m_ui.indirectLightingMode == IndirectLightingMode::ReStirGI)
        {
            ReSTIRDI_InitialSamplingParameters indirectReSTIRDISamplingParams = m_ui.lightingSettings.brdfptParams.secondarySurfaceReSTIRDIParams.initialSamplingParams;
            bool enabled = (indirectReSTIRDISamplingParams.localLightSamplingMode == ReSTIRDI_LocalLightSamplingMode::Power_RIS) ||
                           (indirectReSTIRDISamplingParams.localLightSamplingMode == ReSTIRDI_LocalLightSamplingMode::ReGIR_RIS && m_isContext->GetReGIRContext().IsLocalLightPowerRISEnable());
            if (enabled)
                return true;
        }
        return m_isContext->IsLocalLightPowerRISEnabled();
    }

    void RenderScene(nvrhi::IFramebuffer* framebuffer) override
    {
        if (m_frameStepMode == FrameStepMode::Wait)
        {
            nvrhi::TextureHandle finalImage;

            if (m_ui.enableToneMapping)
                finalImage = m_renderTargets->LdrColor;
            else
                finalImage = m_renderTargets->HdrColor;

            m_commandList->open();

            m_CommonPasses->BlitTexture(m_commandList, framebuffer, finalImage, &m_bindingCache);

            m_commandList->close();
            GetDevice()->executeCommandList(m_commandList);

            return;
        }

        if (m_frameStepMode == FrameStepMode::Step)
            m_frameStepMode = FrameStepMode::Wait;

        const engine::PerspectiveCamera* activeCamera = nullptr;
        uint effectiveFrameIndex = m_renderFrameIndex;

        if (m_ui.animationFrame.has_value())
        {
            const float animationTime = float(m_ui.animationFrame.value()) * (1.f / 240.f);
            
            auto* animation = m_scene->GetBenchmarkAnimation();
            if (animation && animationTime < animation->GetDuration())
            {
                (void)animation->Apply(animationTime);
                activeCamera = m_scene->GetBenchmarkCamera();
                effectiveFrameIndex = m_ui.animationFrame.value();
                m_ui.animationFrame = effectiveFrameIndex + 1;
            }
            else
            {
                m_ui.benchmarkResults = m_profiler->GetAsText();
                m_ui.animationFrame.reset();

                if (m_args.benchmark)
                {
                    glfwSetWindowShouldClose(GetDeviceManager()->GetWindow(), GLFW_TRUE);
                    log::info("BENCHMARK RESULTS >>>\n\n%s<<<", m_ui.benchmarkResults.c_str());
                }
            }
        }

        bool exposureResetRequired = false;

        if (m_ui.enableFpsLimit && GetFrameIndex() > 0)
        {
            uint64_t expectedFrametime = 1000000 / m_ui.fpsLimit;

            while (true)
            {
                uint64_t currentFrametime = duration_cast<microseconds>(steady_clock::now() - m_previousFrameTimeStamp).count();

                if(currentFrametime >= expectedFrametime)
                    break;
#ifdef _WIN32
                Sleep(0);
#else
                usleep(100);
#endif
            }
        }

        m_previousFrameTimeStamp = steady_clock::now();

#if WITH_NRD
        if (m_nrd && m_nrd->GetDenoiser() != m_ui.denoisingMethod)
            m_nrd = nullptr; // need to create a new one
#endif

        if (m_ui.resetISContext)
        {
            GetDevice()->waitForIdle();

            m_isContext = nullptr;
            m_rtxdiResources = nullptr;
            m_ui.resetISContext = false;
        }

        if (m_ui.environmentMapDirty == 2)
        {
            LoadEnvironmentMap();
        }

        m_scene->RefreshSceneGraph(GetFrameIndex());

        const auto& fbinfo = framebuffer->getFramebufferInfo();
        uint32_t renderWidth = fbinfo.width;
        uint32_t renderHeight = fbinfo.height;
        if (m_args.renderWidth > 0 && m_args.renderHeight > 0)
        {
            renderWidth = m_args.renderWidth;
            renderHeight = m_args.renderHeight;
        }
        SetupView(renderWidth, renderHeight, activeCamera);
        SetupRenderPasses(renderWidth, renderHeight, exposureResetRequired);
        if (!m_ui.freezeRegirPosition)
            m_regirCenter = m_camera.GetPosition();
        UpdateReSTIRDIContextFromUI();
        UpdateReGIRContextFromUI();
        UpdateReSTIRGIContextFromUI();
#if WITH_DLSS
        if (!m_ui.dlssAvailable && m_ui.aaMode == AntiAliasingMode::DLSS)
            m_ui.aaMode = AntiAliasingMode::TAA;
#endif

        m_gBufferPass->NextFrame();
        m_postprocessGBufferPass->NextFrame();
        m_lightingPasses->NextFrame();
        m_confidencePass->NextFrame();
        m_compositingPass->NextFrame();
        m_visualizationPass->NextFrame();
        m_renderTargets->NextFrame();
        m_glassPass->NextFrame();
        m_scene->NextFrame();
        m_debugVizPasses->NextFrame();
        
        // Advance the TAA jitter offset at half frame rate if accumulation is used with
        // checkerboard rendering. Otherwise, the jitter pattern resonates with the checkerboard,
        // and stipple patterns appear in the accumulated results.
        if (!((m_ui.aaMode == AntiAliasingMode::Accumulation) && (m_isContext->GetReSTIRDIContext().GetStaticParameters().CheckerboardSamplingMode != rtxdi::CheckerboardMode::Off) && (GetFrameIndex() & 1)))
        {
            m_temporalAntiAliasingPass->AdvanceFrame();
        }
        
        bool cameraIsStatic = m_previousViewValid && m_view.GetViewMatrix() == m_viewPrevious.GetViewMatrix();
        if (cameraIsStatic && (m_ui.aaMode == AntiAliasingMode::Accumulation) && !m_ui.resetAccumulation)
        {
            m_ui.numAccumulatedFrames += 1;

            if (m_ui.framesToAccumulate > 0)
                m_ui.numAccumulatedFrames = std::min(m_ui.numAccumulatedFrames, m_ui.framesToAccumulate);

            m_profiler->EnableAccumulation(true);
        }
        else
        {
            m_ui.numAccumulatedFrames = 1;
            m_profiler->EnableAccumulation(m_ui.animationFrame.has_value());
        }

        float accumulationWeight = 1.f / (float)m_ui.numAccumulatedFrames;

        m_profiler->ResolvePreviousFrame();
        
        int materialIndex = m_profiler->GetMaterialReadback();
        if (materialIndex >= 0)
        {
            for (const auto& material : m_scene->GetSceneGraph()->GetMaterials())
            {
                if (material->materialID == materialIndex)
                {
                    m_ui.resources->selectedMaterial = material;
                    break;
                }
            }
        }
        
        if (m_ui.environmentMapIndex >= 0)
        {
            if (m_environmentMap)
            {
                m_environmentLight->textureIndex = m_environmentMap->bindlessDescriptor.Get();
                const auto& textureDesc = m_environmentMap->texture->getDesc();
                m_environmentLight->textureSize = uint2(textureDesc.width, textureDesc.height);
            }
            else
            {
                m_environmentLight->textureIndex = m_renderEnvironmentMapPass->GetTextureIndex();
                const auto& textureDesc = m_renderEnvironmentMapPass->GetTexture()->getDesc();
                m_environmentLight->textureSize = uint2(textureDesc.width, textureDesc.height);
            }
            m_environmentLight->radianceScale = ::exp2f(m_ui.environmentIntensityBias);
            m_environmentLight->rotation = m_ui.environmentRotation / 360.f;  //  +/- 0.5
            m_sunLight->irradiance = (m_ui.environmentMapIndex > 0) ? 0.f : 1.f;
        }
        else
        {
            m_environmentLight->textureIndex = -1;
            m_sunLight->irradiance = 0.f;
        }
        
#if WITH_NRD
        if (!(m_nrd && m_nrd->IsAvailable()))
            m_ui.enableDenoiser = false;

        uint32_t denoiserMode = (m_ui.enableDenoiser)
            ? (m_ui.denoisingMethod == nrd::Denoiser::RELAX_DIFFUSE_SPECULAR) ? DENOISER_MODE_RELAX : DENOISER_MODE_REBLUR
            : DENOISER_MODE_OFF;
#else
        m_ui.enableDenoiser = false;
        uint32_t denoiserMode = DENOISER_MODE_OFF;
#endif

        m_commandList->open();

        m_profiler->BeginFrame(m_commandList);

        AssignIesProfiles(m_commandList);
        m_scene->RefreshBuffers(m_commandList, GetFrameIndex());
        m_rtxdiResources->InitializeNeighborOffsets(m_commandList, m_isContext->GetNeighborOffsetCount());

        if (m_framesSinceAnimation < 2)
        {
            ProfilerScope scope(*m_profiler, m_commandList, ProfilerSection::TlasUpdate);

            m_scene->UpdateSkinnedMeshBLASes(m_commandList, GetFrameIndex());
            m_scene->BuildTopLevelAccelStruct(m_commandList);
        }
        m_commandList->compactBottomLevelAccelStructs();

        if (m_ui.environmentMapDirty)
        {
            ProfilerScope scope(*m_profiler, m_commandList, ProfilerSection::EnvironmentMap);

            if (m_ui.environmentMapIndex == 0)
            {
                donut::render::SkyParameters params;
                m_renderEnvironmentMapPass->Render(m_commandList, *m_sunLight, params);
            }
            
            m_environmentMapPdfMipmapPass->Process(m_commandList);

            m_ui.environmentMapDirty = 0;
        }

        nvrhi::utils::ClearColorAttachment(m_commandList, framebuffer, 0, nvrhi::Color(0.f));

        {
            ProfilerScope scope(*m_profiler, m_commandList, ProfilerSection::GBufferFill);

            GBufferSettings gbufferSettings = m_ui.gbufferSettings;
            float upscalingLodBias = ::log2f(m_view.GetViewport().width() / m_upscaledView.GetViewport().width());
            gbufferSettings.textureLodBias += upscalingLodBias;

            if (m_ui.rasterizeGBuffer)
                m_rasterizedGBufferPass->Render(m_commandList, m_view, m_viewPrevious, *m_renderTargets, m_ui.gbufferSettings);
            else
                m_gBufferPass->Render(m_commandList, m_view, m_viewPrevious, m_ui.gbufferSettings);

            m_postprocessGBufferPass->Render(m_commandList, m_view);
        }

        // The light indexing members of frameParameters are written by PrepareLightsPass below
        rtxdi::ReSTIRDIContext& restirDIContext = m_isContext->GetReSTIRDIContext();
        restirDIContext.SetFrameIndex(effectiveFrameIndex);
        m_isContext->GetReSTIRGIContext().SetFrameIndex(effectiveFrameIndex);

        {
            ProfilerScope scope(*m_profiler, m_commandList, ProfilerSection::MeshProcessing);
            
            RTXDI_LightBufferParameters lightBufferParams = m_prepareLightsPass->Process(
                m_commandList,
                restirDIContext,
                m_scene->GetSceneGraph()->GetLights(),
                m_environmentMapPdfMipmapPass != nullptr && m_ui.environmentMapImportanceSampling);
            m_isContext->SetLightBufferParams(lightBufferParams);

            auto initialSamplingParams = restirDIContext.GetInitialSamplingParameters();
            initialSamplingParams.environmentMapImportanceSampling = lightBufferParams.environmentLightParams.lightPresent;
            m_ui.restirDI.initialSamplingParams.environmentMapImportanceSampling = initialSamplingParams.environmentMapImportanceSampling;
            restirDIContext.SetInitialSamplingParameters(initialSamplingParams);
        }

        if (IsLocalLightPowerRISEnabled())
        {
            ProfilerScope scope(*m_profiler, m_commandList, ProfilerSection::LocalLightPdfMap);
            
            m_localLightPdfMipmapPass->Process(m_commandList);
        }


#if WITH_NRD
        if (restirDIContext.GetStaticParameters().CheckerboardSamplingMode != rtxdi::CheckerboardMode::Off)
        {
            m_ui.reblurSettings.checkerboardMode = nrd::CheckerboardMode::BLACK;
            m_ui.relaxSettings.checkerboardMode = nrd::CheckerboardMode::BLACK;
        }
        else
        {
            m_ui.reblurSettings.checkerboardMode = nrd::CheckerboardMode::OFF;
            m_ui.relaxSettings.checkerboardMode = nrd::CheckerboardMode::OFF;
        }
#endif
        
        LightingPasses::RenderSettings lightingSettings = m_ui.lightingSettings;
        lightingSettings.enablePreviousTLAS &= m_ui.enableAnimations;
        lightingSettings.enableAlphaTestedGeometry = m_ui.gbufferSettings.enableAlphaTestedGeometry;
        lightingSettings.enableTransparentGeometry = m_ui.gbufferSettings.enableTransparentGeometry;
#if WITH_NRD
        lightingSettings.reblurDiffHitDistanceParams = &m_ui.reblurSettings.hitDistanceParameters;
        lightingSettings.reblurSpecHitDistanceParams = &m_ui.reblurSettings.hitDistanceParameters;
        lightingSettings.denoiserMode = denoiserMode;
#else
        lightingSettings.denoiserMode = DENOISER_MODE_OFF;
#endif
        if (lightingSettings.denoiserMode == DENOISER_MODE_OFF)
            lightingSettings.enableGradients = false;

        const bool checkerboard = restirDIContext.GetStaticParameters().CheckerboardSamplingMode != rtxdi::CheckerboardMode::Off;

        bool enableDirectReStirPass = m_ui.directLightingMode == DirectLightingMode::ReStir;
        bool enableBrdfAndIndirectPass = m_ui.directLightingMode == DirectLightingMode::Brdf || m_ui.indirectLightingMode != IndirectLightingMode::None;
        bool enableIndirect = m_ui.indirectLightingMode != IndirectLightingMode::None;

        // When indirect lighting is enabled, we don't want ReSTIR to be the NRD front-end,
        // it should just write out the raw color data.
        ReSTIRDI_ShadingParameters restirDIShadingParams = m_isContext->GetReSTIRDIContext().GetShadingParameters();
        restirDIShadingParams.enableDenoiserInputPacking = !enableIndirect;
        m_isContext->GetReSTIRDIContext().SetShadingParameters(restirDIShadingParams);

        if (!enableDirectReStirPass)
        {
            // Secondary resampling can only be done as a post-process of ReSTIR direct lighting
            lightingSettings.brdfptParams.enableSecondaryResampling = false;

            // Gradients are only produced by the direct ReSTIR pass
            lightingSettings.enableGradients = false;
        }

        if (enableDirectReStirPass || enableIndirect)
        {
            m_lightingPasses->PrepareForLightSampling(m_commandList,
                *m_isContext,
                m_view, m_viewPrevious,
                lightingSettings,
                /* enableAccumulation = */ m_ui.aaMode == AntiAliasingMode::Accumulation);
        }

        if (enableDirectReStirPass)
        {
            m_commandList->clearTextureFloat(m_renderTargets->Gradients, nvrhi::AllSubresources, nvrhi::Color(0.f));

            m_lightingPasses->RenderDirectLighting(m_commandList,
                restirDIContext,
                m_view,
                lightingSettings);

            // Post-process the gradients into a confidence buffer usable by NRD
            if (lightingSettings.enableGradients)
            {
                m_filterGradientsPass->Render(m_commandList, m_view, checkerboard);
                m_confidencePass->Render(m_commandList, m_view, lightingSettings.gradientLogDarknessBias, lightingSettings.gradientSensitivity, lightingSettings.confidenceHistoryLength, checkerboard);
            }
        }

        if (enableBrdfAndIndirectPass)
        {
            restirDIShadingParams = m_isContext->GetReSTIRDIContext().GetShadingParameters();
            restirDIShadingParams.enableDenoiserInputPacking = true;
            m_isContext->GetReSTIRDIContext().SetShadingParameters(restirDIShadingParams);

            bool enableReSTIRGI = m_ui.indirectLightingMode == IndirectLightingMode::ReStirGI;

            m_lightingPasses->RenderBrdfRays(
                m_commandList,
                *m_isContext,
                m_view, m_viewPrevious,
                lightingSettings,
                m_ui.gbufferSettings,
                *m_environmentLight,
                /* enableIndirect = */ enableIndirect,
                /* enableAdditiveBlend = */ enableDirectReStirPass,
                /* enableEmissiveSurfaces = */ m_ui.directLightingMode == DirectLightingMode::Brdf,
                /* enableAccumulation = */ m_ui.aaMode == AntiAliasingMode::Accumulation,
                enableReSTIRGI
                );
        }

        // If none of the passes above were executed, clear the textures to avoid stale data there.
        // It's a weird mode but it can be selected from the UI.
        if (!enableDirectReStirPass && !enableBrdfAndIndirectPass)
        {
            m_commandList->clearTextureFloat(m_renderTargets->DiffuseLighting, nvrhi::AllSubresources, nvrhi::Color(0.f));
            m_commandList->clearTextureFloat(m_renderTargets->SpecularLighting, nvrhi::AllSubresources, nvrhi::Color(0.f));
        }
        
#if WITH_NRD
        if (m_ui.enableDenoiser)
        {
            ProfilerScope scope(*m_profiler, m_commandList, ProfilerSection::Denoising);
            m_commandList->beginMarker("Denoising");

            const void* methodSettings = (m_ui.denoisingMethod == nrd::Denoiser::RELAX_DIFFUSE_SPECULAR)
                ? (void*)&m_ui.relaxSettings
                : (void*)&m_ui.reblurSettings;

            m_nrd->RunDenoiserPasses(m_commandList, *m_renderTargets, m_view, m_viewPrevious, GetFrameIndex(), lightingSettings.enableGradients, methodSettings, m_ui.debug);
            
            m_commandList->endMarker();
        }
#endif

        m_compositingPass->Render(
            m_commandList,
            m_view,
            m_viewPrevious,
            denoiserMode,
            checkerboard,
            m_ui,
            *m_environmentLight);

        if (m_ui.gbufferSettings.enableTransparentGeometry)
        {
            ProfilerScope scope(*m_profiler, m_commandList, ProfilerSection::Glass);

            m_glassPass->Render(m_commandList, m_view,
                *m_environmentLight,
                m_ui.gbufferSettings.normalMapScale,
                m_ui.gbufferSettings.enableMaterialReadback,
                m_ui.gbufferSettings.materialReadbackPosition);
        }

        Resolve(m_commandList, accumulationWeight);

        if (m_ui.enableBloom)
        {
#if WITH_DLSS
            // Use the unresolved image for bloom when DLSS is active because DLSS can modify HDR values significantly and add bloom flicker.
            nvrhi::ITexture* bloomSource = (m_ui.aaMode == AntiAliasingMode::DLSS && m_ui.resolutionScale == 1.f)
                ? m_renderTargets->HdrColor
                : m_renderTargets->ResolvedColor;
#else
            nvrhi::ITexture* bloomSource = m_RenderTargets->ResolvedColor;
#endif

            m_bloomPass->Render(m_commandList, m_renderTargets->ResolvedFramebuffer, m_upscaledView, bloomSource, 32.f, 0.005f);
        }

        // Reference image functionality:
        {
            // When the camera is moved, discard the previously stored image, if any, and disable its display.
            if (!cameraIsStatic)
            {
                m_ui.referenceImageCaptured = false;
                m_ui.referenceImageSplit = 0.f;
            }

            // When the user clicks the "Store" button, copy the ResolvedColor texture into ReferenceColor.
            if (m_ui.storeReferenceImage)
            {
                m_commandList->copyTexture(m_renderTargets->ReferenceColor, nvrhi::TextureSlice(), m_renderTargets->ResolvedColor, nvrhi::TextureSlice());
                m_ui.storeReferenceImage = false;
                m_ui.referenceImageCaptured = true;
            }

            // When the "Split Display" parameter is nonzero, show a portion of the previously stored
            // ReferenceColor texture on the left side of the screen by copying it into the ResolvedColor texture.
            if (m_ui.referenceImageSplit > 0.f)
            {
                engine::BlitParameters blitParams;
                blitParams.sourceTexture = m_renderTargets->ReferenceColor;
                blitParams.sourceBox.m_maxs = float2(m_ui.referenceImageSplit, 1.f);
                blitParams.targetFramebuffer = m_renderTargets->ResolvedFramebuffer->GetFramebuffer(nvrhi::AllSubresources);
                blitParams.targetBox = blitParams.sourceBox;
                blitParams.sampler = engine::BlitSampler::Point;
                m_CommonPasses->BlitTexture(m_commandList, blitParams, &m_bindingCache);
            }
        }

        if(m_ui.enableToneMapping)
        { // Tone mapping
            render::ToneMappingParameters ToneMappingParams;
            ToneMappingParams.minAdaptedLuminance = 0.002f;
            ToneMappingParams.maxAdaptedLuminance = 0.2f;
            ToneMappingParams.exposureBias = m_ui.exposureBias;
            ToneMappingParams.eyeAdaptationSpeedUp = 2.0f;
            ToneMappingParams.eyeAdaptationSpeedDown = 1.0f;

            if (exposureResetRequired)
            {
                ToneMappingParams.eyeAdaptationSpeedUp = 0.f;
                ToneMappingParams.eyeAdaptationSpeedDown = 0.f;
            }

            m_toneMappingPass->SimpleRender(m_commandList, ToneMappingParams, m_upscaledView, m_renderTargets->ResolvedColor);
        }
        else
        {
            m_CommonPasses->BlitTexture(m_commandList, m_renderTargets->LdrFramebuffer->GetFramebuffer(m_upscaledView), m_renderTargets->ResolvedColor, &m_bindingCache);
        }

        if (m_ui.visualizationMode != VIS_MODE_NONE)
        {
            bool haveSignal = true;
            uint32_t inputBufferIndex = 0;
            switch(m_ui.visualizationMode)
            {
            case VIS_MODE_DENOISED_DIFFUSE:
            case VIS_MODE_DENOISED_SPECULAR:
                haveSignal = m_ui.enableDenoiser;
                break;

            case VIS_MODE_DIFFUSE_CONFIDENCE:
            case VIS_MODE_SPECULAR_CONFIDENCE:
                haveSignal = m_ui.lightingSettings.enableGradients && m_ui.enableDenoiser;
                break;

            case VIS_MODE_RESERVOIR_WEIGHT:
            case VIS_MODE_RESERVOIR_M:
                inputBufferIndex = m_lightingPasses->GetOutputReservoirBufferIndex();
                haveSignal = m_ui.directLightingMode == DirectLightingMode::ReStir;
                break;
                
            case VIS_MODE_GI_WEIGHT:
            case VIS_MODE_GI_M:
                inputBufferIndex = m_lightingPasses->GetGIOutputReservoirBufferIndex();
                haveSignal = m_ui.indirectLightingMode == IndirectLightingMode::ReStirGI;
                break;
            }

            if (haveSignal)
            {
                m_visualizationPass->Render(
                    m_commandList,
                    m_renderTargets->LdrFramebuffer->GetFramebuffer(m_upscaledView),
                    m_view,
                    m_upscaledView,
                    *m_isContext,
                    inputBufferIndex,
                    m_ui.visualizationMode,
                    m_ui.aaMode == AntiAliasingMode::Accumulation);
            }
        }

        switch (m_ui.debugRenderOutputBuffer)
        {
            case DebugRenderOutput::LDRColor:
                m_CommonPasses->BlitTexture(m_commandList, framebuffer, m_renderTargets->LdrColor, &m_bindingCache);
                break;
            case DebugRenderOutput::Depth:
                m_CommonPasses->BlitTexture(m_commandList, framebuffer, m_renderTargets->Depth, &m_bindingCache);
                break;
            case GBufferDiffuseAlbedo:
                m_debugVizPasses->RenderUnpackedDiffuseAlbeo(m_commandList, m_upscaledView);
                m_CommonPasses->BlitTexture(m_commandList, framebuffer, m_renderTargets->DebugColor, &m_bindingCache);
                break;
            case GBufferSpecularRough:
                m_debugVizPasses->RenderUnpackedSpecularRoughness(m_commandList, m_upscaledView);
                m_CommonPasses->BlitTexture(m_commandList, framebuffer, m_renderTargets->DebugColor, &m_bindingCache);
                break;
            case GBufferNormals:
                m_debugVizPasses->RenderUnpackedNormals(m_commandList, m_upscaledView);
                m_CommonPasses->BlitTexture(m_commandList, framebuffer, m_renderTargets->DebugColor, &m_bindingCache);
                break;
            case GBufferGeoNormals:
                m_debugVizPasses->RenderUnpackedGeoNormals(m_commandList, m_upscaledView);
                m_CommonPasses->BlitTexture(m_commandList, framebuffer, m_renderTargets->DebugColor, &m_bindingCache);
                break;
            case GBufferEmissive:
                m_CommonPasses->BlitTexture(m_commandList, framebuffer, m_renderTargets->GBufferEmissive, &m_bindingCache);
                break;
            case DiffuseLighting:
                m_CommonPasses->BlitTexture(m_commandList, framebuffer, m_renderTargets->DiffuseLighting, &m_bindingCache);
                break;
            case SpecularLighting:
                m_CommonPasses->BlitTexture(m_commandList, framebuffer, m_renderTargets->SpecularLighting, &m_bindingCache);
                break;
            case DenoisedDiffuseLighting:
                m_CommonPasses->BlitTexture(m_commandList, framebuffer, m_renderTargets->DenoisedDiffuseLighting, &m_bindingCache);
                break;
            case DenoisedSpecularLighting:
                m_CommonPasses->BlitTexture(m_commandList, framebuffer, m_renderTargets->DenoisedSpecularLighting, &m_bindingCache);
                break;
            case RestirLuminance:
                m_CommonPasses->BlitTexture(m_commandList, framebuffer, m_renderTargets->RestirLuminance, &m_bindingCache);
                break;
            case PrevRestirLuminance:
                m_CommonPasses->BlitTexture(m_commandList, framebuffer, m_renderTargets->PrevRestirLuminance, &m_bindingCache);
                break;
            case DiffuseConfidence:
                m_CommonPasses->BlitTexture(m_commandList, framebuffer, m_renderTargets->DiffuseConfidence, &m_bindingCache);
                break;
            case SpecularConfidence:
                m_CommonPasses->BlitTexture(m_commandList, framebuffer, m_renderTargets->SpecularConfidence, &m_bindingCache);
                break;
            case MotionVectors:
                m_CommonPasses->BlitTexture(m_commandList, framebuffer, m_renderTargets->MotionVectors, &m_bindingCache);
        }
        
        m_profiler->EndFrame(m_commandList);

        m_commandList->close();
        GetDevice()->executeCommandList(m_commandList);

        if (!m_args.saveFrameFileName.empty() && m_renderFrameIndex == m_args.saveFrameIndex)
        {
            bool success = SaveTexture(GetDevice(), m_renderTargets->LdrColor, m_args.saveFrameFileName.c_str());

            g_ExitCode = success ? 0 : 1;
            
            glfwSetWindowShouldClose(GetDeviceManager()->GetWindow(), 1);
        }
        
        m_ui.gbufferSettings.enableMaterialReadback = false;
        
        if (m_ui.enableAnimations)
            m_framesSinceAnimation = 0;
        else
            m_framesSinceAnimation++;
        
        m_viewPrevious = m_view;
        m_previousViewValid = true;
        m_ui.resetAccumulation = false;
        ++m_renderFrameIndex;
    }

private:
    nvrhi::CommandListHandle m_commandList;

    nvrhi::BindingLayoutHandle m_bindlessLayout;

    std::shared_ptr<vfs::RootFileSystem> m_rootFs;
    std::shared_ptr<engine::ShaderFactory> m_shaderFactory;
    std::shared_ptr<SampleScene> m_scene;
    std::shared_ptr<engine::DescriptorTableManager> m_descriptorTableManager;
    std::unique_ptr<render::ToneMappingPass> m_toneMappingPass;
    std::unique_ptr<render::TemporalAntiAliasingPass> m_temporalAntiAliasingPass;
    std::unique_ptr<render::BloomPass> m_bloomPass;
    std::shared_ptr<RenderTargets> m_renderTargets;
    app::FirstPersonCamera m_camera;
    engine::PlanarView m_view;
    engine::PlanarView m_viewPrevious;
    engine::PlanarView m_upscaledView;
    std::shared_ptr<engine::DirectionalLight> m_sunLight;
    std::shared_ptr<EnvironmentLight> m_environmentLight;
    std::shared_ptr<engine::LoadedTexture> m_environmentMap;
    engine::BindingCache m_bindingCache;

    std::unique_ptr<rtxdi::ImportanceSamplingContext> m_isContext;
    std::unique_ptr<RaytracedGBufferPass> m_gBufferPass;
    std::unique_ptr<RasterizedGBufferPass> m_rasterizedGBufferPass;
    std::unique_ptr<PostprocessGBufferPass> m_postprocessGBufferPass;
    std::unique_ptr<GlassPass> m_glassPass;
    std::unique_ptr<FilterGradientsPass> m_filterGradientsPass;
    std::unique_ptr<ConfidencePass> m_confidencePass;
    std::unique_ptr<CompositingPass> m_compositingPass;
    std::unique_ptr<AccumulationPass> m_accumulationPass;
    std::unique_ptr<PrepareLightsPass> m_prepareLightsPass;
    std::unique_ptr<RenderEnvironmentMapPass> m_renderEnvironmentMapPass;
    std::unique_ptr<GenerateMipsPass> m_environmentMapPdfMipmapPass;
    std::unique_ptr<GenerateMipsPass> m_localLightPdfMipmapPass;
    std::unique_ptr<LightingPasses> m_lightingPasses;
    std::unique_ptr<VisualizationPass> m_visualizationPass;
    std::unique_ptr<RtxdiResources> m_rtxdiResources;
    std::unique_ptr<engine::IesProfileLoader> m_iesProfileLoader;
    std::shared_ptr<Profiler> m_profiler;
    std::unique_ptr<DebugVizPasses> m_debugVizPasses;

    uint32_t m_renderFrameIndex = 0;

#if WITH_NRD
    std::unique_ptr<NrdIntegration> m_nrd;
#endif

#if WITH_DLSS
    std::unique_ptr<DLSS> m_dlss;
#endif

    UIData& m_ui;
    CommandLineArguments& m_args;
    uint m_framesSinceAnimation = 0;
    bool m_previousViewValid = false;
    time_point<steady_clock> m_previousFrameTimeStamp;

    std::vector<std::shared_ptr<engine::IesProfile>> m_iesProfiles;

    dm::float3 m_regirCenter;

    enum class FrameStepMode
    {
        Disabled,
        Wait,
        Step
    };

    FrameStepMode m_frameStepMode = FrameStepMode::Disabled;
};

#if defined(_WIN32) && !defined(IS_CONSOLE_APP)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
#else
int main(int argc, char** argv)
#endif
{
    log::SetCallback(&ApplicationLogCallback);
    
    app::DeviceCreationParameters deviceParams;
    deviceParams.swapChainBufferCount = 3;
    deviceParams.enableRayTracingExtensions = true;
    deviceParams.backBufferWidth = 1920;
    deviceParams.backBufferHeight = 1080;
    deviceParams.vsyncEnabled = true;
    deviceParams.infoLogSeverity = log::Severity::Debug;

    UIData ui;
    CommandLineArguments args;

#if defined(_WIN32) && !defined(IS_CONSOLE_APP)
    ProcessCommandLine(__argc, __argv, deviceParams, ui, args);
#else
    ProcessCommandLine(argc, argv, deviceParams, ui, args);
#endif

    if (args.verbose)
        log::SetMinSeverity(log::Severity::Debug);
    
    app::DeviceManager* deviceManager = app::DeviceManager::Create(args.graphicsApi);

#if DONUT_WITH_VULKAN
    if (args.graphicsApi == nvrhi::GraphicsAPI::VULKAN)
    {
        // Set the extra device feature bit(s)
        deviceParams.deviceCreateInfoCallback = [](VkDeviceCreateInfo& info) {
            auto features = const_cast<VkPhysicalDeviceFeatures*>(info.pEnabledFeatures);
            features->fragmentStoresAndAtomics = VK_TRUE;
#if WITH_DLSS
            features->shaderStorageImageWriteWithoutFormat = VK_TRUE;
#endif
        };

#if WITH_DLSS
        DLSS::GetRequiredVulkanExtensions(
            deviceParams.optionalVulkanInstanceExtensions,
            deviceParams.optionalVulkanDeviceExtensions);

        // Currently, DLSS on Vulkan produces these validation errors. Silence them.
        // Re-evaluate when updating DLSS.

        // VkDeviceCreateInfo->ppEnabledExtensionNames must not contain both VK_KHR_buffer_device_address and VK_EXT_buffer_device_address
        deviceParams.ignoredVulkanValidationMessageLocations.push_back(0xffffffff83a6bda8);
        
        // If VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT is set, bufferDeviceAddress must be enabled.
        deviceParams.ignoredVulkanValidationMessageLocations.push_back(0xfffffffff972dfbf);

        // vkCmdCuLaunchKernelNVX: required parameter pLaunchInfo->pParams specified as NULL.
        deviceParams.ignoredVulkanValidationMessageLocations.push_back(0x79de34d4);
#endif
}
#endif

    const char* apiString = nvrhi::utils::GraphicsAPIToString(deviceManager->GetGraphicsAPI());

    std::string windowTitle = std::string(g_ApplicationTitle) + " (" + std::string(apiString) + ")";
    
    log::SetErrorMessageCaption(windowTitle.c_str());

#ifdef _WIN32
    // Disable Window scaling.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif

    if (!deviceManager->CreateWindowDeviceAndSwapChain(deviceParams, windowTitle.c_str()))
    {
        log::error("Cannot initialize a %s graphics device.", apiString);
        return 1;
    }

    bool rayPipelineSupported = deviceManager->GetDevice()->queryFeatureSupport(nvrhi::Feature::RayTracingPipeline);
    bool rayQuerySupported = deviceManager->GetDevice()->queryFeatureSupport(nvrhi::Feature::RayQuery);

    if (!rayPipelineSupported && !rayQuerySupported)
    {
        log::error("The GPU (%s) or its driver does not support ray tracing.", deviceManager->GetRendererString());
        return 1;
    }

#if DONUT_WITH_DX12
    if (args.graphicsApi == nvrhi::GraphicsAPI::D3D12 && args.disableBackgroundOptimization)
    {
        // On DX12, optionally disable the background shader optimization because it leads to stutter on some NV driver versions (496.61 specifically).
        
        nvrhi::RefCountPtr<ID3D12Device> device = (ID3D12Device*)deviceManager->GetDevice()->getNativeObject(nvrhi::ObjectTypes::D3D12_Device);
        nvrhi::RefCountPtr<ID3D12Device6> device6;

        if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&device6))))
        {
            HRESULT hr = device6->SetBackgroundProcessingMode(
                D3D12_BACKGROUND_PROCESSING_MODE_DISABLE_PROFILING_BY_SYSTEM,
                D3D12_MEASUREMENTS_ACTION_DISCARD_PREVIOUS,
                nullptr, nullptr);

            if (FAILED(hr))
            {
                log::info("Call to ID3D12Device6::SetBackgroundProcessingMode(...) failed, HRESULT = 0x%08x. Expect stutter.", hr);
            }
        }
    }
#endif

    {
        SceneRenderer sceneRenderer(deviceManager, ui, args);
        if (sceneRenderer.Init())
        {
            UserInterface userInterface(deviceManager, *sceneRenderer.GetRootFs(), ui);
            userInterface.Init(sceneRenderer.GetShaderFactory());

            deviceManager->AddRenderPassToBack(&sceneRenderer);
            deviceManager->AddRenderPassToBack(&userInterface);
            deviceManager->RunMessageLoop();
            deviceManager->GetDevice()->waitForIdle();
            deviceManager->RemoveRenderPass(&sceneRenderer);
            deviceManager->RemoveRenderPass(&userInterface);
        }

        // Clear the shared pointers from 'ui' to graphics objects
        ui.resources.reset();
    }
    
    deviceManager->Shutdown();

    delete deviceManager;

    return g_ExitCode;
}
