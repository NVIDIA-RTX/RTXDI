
include("${DONUT_PATH}/compileshaders.cmake")

set(shaders
   DebugViz/NDirOctUNorm32Viz.hlsl
   DebugViz/PackedR11G11B10UFloatViz.hlsl
   DebugViz/PackedR8G8B8A8GammaUFloatViz.hlsl
   DenoisingPasses/ComputeGradients.hlsl
   DenoisingPasses/ConfidencePass.hlsl
   DenoisingPasses/FilterGradientsPass.hlsl
   LightingPasses/DI/FusedResampling.hlsl
   LightingPasses/DI/GenerateInitialSamples.hlsl
   LightingPasses/DI/ShadeSamples.hlsl
   LightingPasses/DI/SpatialResampling.hlsl
   LightingPasses/DI/TemporalResampling.hlsl
   LightingPasses/GI/FinalShading.hlsl
   LightingPasses/GI/FusedResampling.hlsl
   LightingPasses/GI/SpatialResampling.hlsl
   LightingPasses/GI/TemporalResampling.hlsl
   LightingPasses/Presampling/PresampleEnvironmentMap.hlsl
   LightingPasses/Presampling/PresampleLights.hlsl
   LightingPasses/Presampling/PresampleReGIR.hlsl
   LightingPasses/BrdfRayTracing.hlsl
   LightingPasses/RtxdiApplicationBridge/RAB_Buffers.hlsli
   LightingPasses/RtxdiApplicationBridge/RAB_LightInfo.hlsli
   LightingPasses/RtxdiApplicationBridge/RAB_LightSample.hlsli
   LightingPasses/RtxdiApplicationBridge/RAB_LightSampling.hlsli
   LightingPasses/RtxdiApplicationBridge/RAB_Material.hlsli
   LightingPasses/RtxdiApplicationBridge/RAB_RandomSamplerState.hlsli
   LightingPasses/RtxdiApplicationBridge/RAB_RayPayload.hlsli
   LightingPasses/RtxdiApplicationBridge/RAB_RTShaders.hlsli
   LightingPasses/RtxdiApplicationBridge/RAB_SpatialHelpers.hlsli
   LightingPasses/RtxdiApplicationBridge/RAB_Surface.hlsli
   LightingPasses/RtxdiApplicationBridge/RAB_VisibilityTest.hlsli
   LightingPasses/RtxdiApplicationBridge/RtxdiApplicationBridge.hlsli
   LightingPasses/ShadeSecondarySurfaces.hlsl
   LightingPasses/ShadingHelpers.hlsli
   AccumulationPass.hlsl
   BRDFPTParameters.h
   CompositingPass.hlsl
   DlssExposure.hlsl
   GBufferHelpers.hlsli
   GlassPass.hlsl
   HelperFunctions.hlsli
   GlassPass.hlsl
   Helperfunctions.hlsli
   LightShaping.hlsli
   PolymorphicLight.hlsli
   PostprocessGBuffer.hlsl
   PrepareLights.hlsl
   PreprocessEnvironmentMap.hlsl
   RasterizedGBuffer.hlsl
   RaytracedGBuffer.hlsl
   RenderEnvironmentMap.hlsl
   SceneGeometry.hlsli
   ShaderParameters.h
   VisualizeConfidence.hlsl
   VisualizeHdrSignals.hlsl)

# Organize MSVS filters (the little folders in the solution explorer) to match the folder structure
foreach(source IN LISTS shaders)
    get_filename_component(source_path "${source}" PATH)
    string(REPLACE "/" "\\" source_path_msvc "${source_path}")
    source_group("${source_path_msvc}" FILES "${source}")
endforeach()

set(project FullSample)
set(folder "RTXDI SDK")
set(shaders_target FullSampleShaders)

add_custom_target(${shaders_target}
    DEPENDS ShaderMake
    SOURCES ${shaders} Shaders.cfg)

if(TARGET NRD)
   set(NRD_OPTIONS -I "${CMAKE_CURRENT_SOURCE_DIR}/../../../external/NRD/Shaders/Include" -D WITH_NRD -D NRD_USE_OCT_NORMAL_ENCODING=0 -D NRD_USE_MATERIAL_ID=0)
else()
   set(NRD_OPTIONS --relaxedInclude NRD.hlsli)
endif()

set (OUTPUT_PATH_BASE "${CMAKE_BINARY_DIR}/bin/shaders/full-sample")

 if (WIN32)
     set (USE_API_OPTION --useAPI)
 else()
     set (USE_API_OPTION "")
 endif()

if (DONUT_WITH_DX12)
   set(DX12_COMPILER_OPTIONS 
      --platform DXIL
      --shaderModel 6_5
      --binaryBlob
      --outputExt .bin
      -I ${DONUT_SHADER_INCLUDE_DIR}
      -I ${RTXDI_RUNTIME_INCLUDE_PATH}
      --relaxedInclude "../Types.h"
      ${NRD_OPTIONS}
      ${USE_API_OPTION}
      --compiler ${DXC_PATH})

   add_custom_command(TARGET ${shaders_target} PRE_BUILD
                     COMMAND ShaderMake
                              --config ${CMAKE_CURRENT_SOURCE_DIR}/Shaders.cfg
                              --out ${OUTPUT_PATH_BASE}/dxil
                              ${DX12_COMPILER_OPTIONS})
endif()

if (DONUT_WITH_VULKAN)
   set(VULKAN_COMPILER_OPTIONS
      --platform SPIRV
      --shaderModel 6_5
      --vulkanVersion 1.2
      --binaryBlob
      --outputExt .bin
      -I ${DONUT_SHADER_INCLUDE_DIR}
      -I ${RTXDI_RUNTIME_INCLUDE_PATH}
      --relaxedInclude "../Types.h"
      ${NRD_OPTIONS}
      ${USE_API_OPTION}
      ${NVRHI_DEFAULT_VK_REGISTER_OFFSETS}
      -D SPIRV
      --compiler ${DXC_SPIRV_PATH})

   add_custom_command(TARGET ${shaders_target} PRE_BUILD
                     COMMAND ShaderMake
                              --config ${CMAKE_CURRENT_SOURCE_DIR}/Shaders.cfg
                              --out ${OUTPUT_PATH_BASE}/spirv
                              ${VULKAN_COMPILER_OPTIONS})
endif()

set_target_properties(${shaders_target} PROPERTIES FOLDER ${folder})
set_source_files_properties(${shaders} PROPERTIES VS_TOOL_OVERRIDE "None") 
