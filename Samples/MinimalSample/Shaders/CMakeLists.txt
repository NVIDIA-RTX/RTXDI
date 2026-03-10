
include("${DONUT_PATH}/compileshaders.cmake")
file(GLOB shaders "*.hlsl" "*.hlsli" "*.h")
set(shaders
   RtxdiApplicationBridge/RAB_Buffers.hlsli
   RtxdiApplicationBridge/RAB_LightInfo.hlsli
   RtxdiApplicationBridge/RAB_LightSample.hlsli
   RtxdiApplicationBridge/RAB_LightSampling.hlsli
   RtxdiApplicationBridge/RAB_Material.hlsli
   RtxdiApplicationBridge/RAB_RayPayload.hlsli
   RtxdiApplicationBridge/RAB_RTShaders.hlsli
   RtxdiApplicationBridge/RAB_SpatialHelpers.hlsli
   RtxdiApplicationBridge/RAB_Surface.hlsli
   RtxdiApplicationBridge/RAB_VisibilityTest.hlsli
   RtxdiApplicationBridge/RtxdiApplicationBridge.hlsli
   GBufferHelpers.hlsli
   HelperFunctions.hlsli
   PrepareLights.hlsl
   PrimaryRays.hlsli
   Render.hlsl
   SceneGeometry.hlsli
   ShaderParameters.h
   TriangleLight.hlsli)

# Organize MSVS filters (the little folders in the solution explorer) to match the folder structure
foreach(source IN LISTS shaders)
    get_filename_component(source_path "${source}" PATH)
    string(REPLACE "/" "\\" source_path_msvc "${source_path}")
    source_group("${source_path_msvc}" FILES "${source}")
endforeach()

set(project MinimalSample)
set(folder "RTXDI SDK")
set(shaders_target MinimalSampleShaders)

add_custom_target(${shaders_target}
	DEPENDS ShaderMake
	SOURCES ${shaders} Shaders.cfg)

set (OUTPUT_PATH_BASE "${CMAKE_BINARY_DIR}/bin/shaders/minimal-sample")
 
if (DONUT_WITH_DX12)
   set(DX12_COMPILER_OPTIONS 
      --platform DXIL
      --shaderModel 6_5
      --binaryBlob
      --outputExt .bin
      -I ${DONUT_SHADER_INCLUDE_DIR}
      -I ${RTXDI_RUNTIME_INCLUDE_PATH}
      --relaxedInclude "../Types.h"
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
      -D SPIRV
      ${NVRHI_DEFAULT_VK_REGISTER_OFFSETS}
      --compiler ${DXC_SPIRV_PATH})

   add_custom_command(TARGET ${shaders_target} PRE_BUILD
                     COMMAND ShaderMake
                              --config ${CMAKE_CURRENT_SOURCE_DIR}/Shaders.cfg
                              --out ${OUTPUT_PATH_BASE}/spirv
                              ${VULKAN_COMPILER_OPTIONS})
endif()

set_target_properties(${shaders_target} PROPERTIES FOLDER ${folder})
set_source_files_properties(${shaders} PROPERTIES VS_TOOL_OVERRIDE "None") 
