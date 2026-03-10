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

#ifndef PT_PATH_VIZ_LINES_PASS_H
#define PT_PATH_VIZ_LINES_PASS_H

#include <memory>
#include <vector>

#include <nvrhi/nvrhi.h>
#include <donut/core/math/math.h>
using namespace donut::math;
#include "SharedShaderInclude/ShaderDebug/PTPathViz/PTPathVizConstants.h"

namespace donut::engine
{
    class FramebufferFactory;
    class Scene;
    class ShaderFactory;
    class IView;
}

class RenderTargets;

class PTPathVizLinesPass
{
public:
    PTPathVizLinesPass(
        nvrhi::IDevice* device,
        std::shared_ptr<donut::engine::ShaderFactory> shaderFactory,
        std::shared_ptr<donut::engine::Scene> scene);

    void CreatePipeline(const RenderTargets& renderTargets);

    void CreateBindingSet();

    void EnableDepthTest(bool enable);
    bool IsDepthTestEnabled() const;

    void RenderLines(
        nvrhi::ICommandList* commandList,
        const donut::engine::IView& view,
        nvrhi::BufferHandle lineListBuffer,
        const PTPathVizPaths& paths,
        uint32_t cameraVertexEnabled,
        uint32_t numLinesPerPath,
        uint32_t numPaths);

private:
    void CreateBindingLayout();

    void UpdateConstantBuffer(nvrhi::ICommandList* commandList,
                              const donut::engine::IView& view,
                              const PTPathVizPaths& paths,
                              uint32_t cameraVertexEnabled,
                              uint32_t maxLines);

    void SetGraphicsState(nvrhi::ICommandList* commandList,
                          const donut::engine::IView& view,
                          nvrhi::BufferHandle lineListBuffer);

    void DrawLines(nvrhi::ICommandList* commandList, uint32_t numLines);

    nvrhi::DeviceHandle m_device;

    nvrhi::GraphicsPipelineHandle m_pipelineWithDepth;
    nvrhi::GraphicsPipelineHandle m_pipelineWithoutDepth;
    nvrhi::BindingLayoutHandle m_bindingLayout;
    nvrhi::BindingSetHandle m_bindingSet;

    std::shared_ptr<donut::engine::FramebufferFactory> m_framebufferWithDepth;
    std::shared_ptr<donut::engine::FramebufferFactory> m_framebufferWithoutDepth;

    nvrhi::BufferHandle m_constantBuffer;

    std::shared_ptr<donut::engine::ShaderFactory> m_shaderFactory;
    std::shared_ptr<donut::engine::Scene> m_scene;

    bool m_isCameraToPrimarySurfaceRayEnabled;
    bool m_isDepthTestEnabled;
};

#endif // PT_PATH_VIZ_LINES_PASS_H