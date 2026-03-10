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

#include "../../Profiler.h"

#include <donut/core/math/math.h>
using namespace donut::math;

#include "PTPathVizLinesPass.h"
#include "PTPathVizPreprocessPass.h"
#include "SharedShaderInclude/ShaderDebug/PTPathViz/PTPathVizConstants.h"

#include <nvrhi/nvrhi.h>
#include <memory>

namespace donut::engine
{
    class Scene;
    class CommonRenderPasses;
    class ShaderFactory;
    class IView;
}

class RenderTargets;

class PTPathVizPass
{
public:
    PTPathVizPass(
        nvrhi::IDevice* device,
        std::shared_ptr<donut::engine::ShaderFactory> shaderFactory,
        std::shared_ptr<donut::engine::Scene> scene,
        std::shared_ptr<Profiler> profiler,
        nvrhi::IBindingLayout* bindlessLayout);

    void CreateBindingSets();
    void CreatePipelines(const RenderTargets& renderTargets);

    nvrhi::BufferHandle GetPathRecordBuffer();
    nvrhi::BufferHandle GetPathVertexCountBuffer();
    nvrhi::BufferHandle GetRenderVertexBuffer();
    nvrhi::BufferHandle GetRenderNormalBuffer();
    nvrhi::BufferHandle GetRenderNEELightBuffer();

    void SetPathsData(const PTPathVizPaths& paths);

    // Call each time you want to read back the data from the path tracer
    // Suggest calling once per click or once per frame depending on desired behavior
    void Update(nvrhi::ICommandList* commandList);

    // Draws the lines
    void Render(
        nvrhi::ICommandList* commandList,
        const donut::engine::IView& view,
        const RenderTargets& renderTargets);
	
    bool IsDepthTestEnabled() const;
    void EnableDepthTest(bool enable);

    bool IsCameraToPrimarySurfaceRayEnabled() const;
    void EnableCameraToPrimarySurfaceRay(bool enable);

private:
    void CreatePathRecordBuffer();
    void CreatePathVertexCountBuffer();
    void CreateRenderVertexBuffers();

    nvrhi::DeviceHandle m_device;

    // Path record buffer for storing path vertex data
    nvrhi::BufferHandle m_pathRecordBuffer;

    // Single value buffer to hold the number of vertices written by the path tracer
    nvrhi::BufferHandle m_pathSetBuffer;

    // Line list buffers for drawing
    nvrhi::BufferHandle m_renderVertexBuffer;
    nvrhi::BufferHandle m_renderNormalBuffer;
    nvrhi::BufferHandle m_renderNEELightBuffer;

    uint32_t m_maxPathLength;
    uint32_t m_maxPaths;

    PTPathVizPaths m_pathsData;

    PTPathVizPreprocessPass m_preprocessPass;
    PTPathVizLinesPass m_linesPass;
	
    bool m_isCameraToPrimarySurfaceRayEnabled;
    bool m_isDepthTestEnabled;
};