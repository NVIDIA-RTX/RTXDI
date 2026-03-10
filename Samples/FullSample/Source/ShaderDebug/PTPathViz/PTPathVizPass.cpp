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

#include "PTPathVizPass.h"

#include "../../RenderTargets.h"
#include "../../Profiler.h"
#include "../../Scene/SampleScene.h"

#include <donut/engine/Scene.h>
#include <donut/engine/FramebufferFactory.h>
#include <donut/engine/ShaderFactory.h>
#include <donut/engine/View.h>
#include <donut/core/log.h>
#include <nvrhi/utils.h>

using namespace donut::math;
#include "SharedShaderInclude/ShaderParameters.h"
#include "SharedShaderInclude/ShaderDebug/PTPathViz/PTPathVizConstants.h"
#include "SharedShaderInclude/ShaderDebug/PTPathViz/PTPathVertexRecord.h"
#include "SharedShaderInclude/ShaderDebug/PTPathViz/PTPathSetRecord.h"

PTPathVizPass::PTPathVizPass(
    nvrhi::IDevice* device,
    std::shared_ptr<donut::engine::ShaderFactory> shaderFactory,
    std::shared_ptr<donut::engine::Scene> scene,
    std::shared_ptr<Profiler> profiler,
    nvrhi::IBindingLayout* bindlessLayout)
    : m_device(device)
    , m_maxPathLength(32)
    , m_maxPaths(48)
    , m_preprocessPass(device, shaderFactory)
    , m_linesPass(PTPathVizLinesPass(device, shaderFactory, scene))
    , m_isCameraToPrimarySurfaceRayEnabled(true)
    , m_isDepthTestEnabled(true)
{
    CreatePathRecordBuffer();
    CreatePathVertexCountBuffer();
    CreateRenderVertexBuffers();
}

void PTPathVizPass::CreateBindingSets()
{
    m_preprocessPass.CreateBindingSet(
        m_pathRecordBuffer,
        m_pathSetBuffer,
        m_renderVertexBuffer,
        m_renderNormalBuffer,
        m_renderNEELightBuffer);
    m_linesPass.CreateBindingSet();
}

void PTPathVizPass::CreatePipelines(const RenderTargets& renderTargets)
{
    m_preprocessPass.CreatePipeline();
    m_linesPass.CreatePipeline(renderTargets);
}

nvrhi::BufferHandle PTPathVizPass::GetPathRecordBuffer()
{
    return m_pathRecordBuffer;
}

nvrhi::BufferHandle PTPathVizPass::GetPathVertexCountBuffer()
{
    return m_pathSetBuffer;
}

nvrhi::BufferHandle PTPathVizPass::GetRenderVertexBuffer()
{
    return m_renderVertexBuffer;
}

nvrhi::BufferHandle PTPathVizPass::GetRenderNormalBuffer()
{
    return m_renderNormalBuffer;
}

nvrhi::BufferHandle PTPathVizPass::GetRenderNEELightBuffer()
{
    return m_renderNEELightBuffer;
}

void PTPathVizPass::SetPathsData(const PTPathVizPaths& paths)
{
    m_pathsData = paths;
}

void PTPathVizPass::Update(nvrhi::ICommandList* commandList)
{
    commandList->beginMarker("PTPathVizPass");

    commandList->commitBarriers();

    m_preprocessPass.ConvertPathRecordToLineListBuffers(commandList, m_maxPaths, m_maxPathLength);

    commandList->endMarker();
}

void PTPathVizPass::Render(
    nvrhi::ICommandList* commandList,
    const donut::engine::IView& view,
    const RenderTargets& renderTargets)
{
    commandList->beginMarker("PTPathVizPass");

    commandList->setResourceStatesForFramebuffer(renderTargets.LdrFramebuffer->GetFramebuffer(nvrhi::AllSubresources));
    commandList->commitBarriers();

    m_linesPass.RenderLines(commandList, view, m_renderVertexBuffer, m_pathsData, m_isCameraToPrimarySurfaceRayEnabled, m_maxPathLength, m_maxPaths);

    if(m_pathsData.normal.enabled) m_linesPass.RenderLines(commandList, view, m_renderNormalBuffer, m_pathsData, m_isCameraToPrimarySurfaceRayEnabled, m_maxPathLength, m_maxPaths);
    if(m_pathsData.nee.enabled) m_linesPass.RenderLines(commandList, view, m_renderNEELightBuffer, m_pathsData, m_isCameraToPrimarySurfaceRayEnabled, m_maxPathLength, m_maxPaths);
    commandList->clearBufferUInt(m_pathSetBuffer, 0);

    commandList->endMarker();
}

bool PTPathVizPass::IsDepthTestEnabled() const
{
    return m_linesPass.IsDepthTestEnabled();
}

void PTPathVizPass::EnableDepthTest(bool enable)
{
    m_linesPass.EnableDepthTest(enable);
}

bool PTPathVizPass::IsCameraToPrimarySurfaceRayEnabled() const
{
    return m_isCameraToPrimarySurfaceRayEnabled;
}

void PTPathVizPass::EnableCameraToPrimarySurfaceRay(bool enable)
{
    m_isCameraToPrimarySurfaceRayEnabled = enable;
}

void PTPathVizPass::CreatePathRecordBuffer()
{
    nvrhi::BufferDesc pathRecordBufferDesc = {};
    pathRecordBufferDesc.byteSize = m_maxPaths * m_maxPathLength * sizeof(PTPathVertexRecord);
    pathRecordBufferDesc.format = nvrhi::Format::UNKNOWN;
    pathRecordBufferDesc.canHaveTypedViews = true;
    pathRecordBufferDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
    pathRecordBufferDesc.debugName = "PTPathVizVertexRecordBuffer";
    pathRecordBufferDesc.canHaveUAVs = true;
    pathRecordBufferDesc.keepInitialState = true;
    pathRecordBufferDesc.structStride = sizeof(PTPathVertexRecord);
    m_pathRecordBuffer = m_device->createBuffer(pathRecordBufferDesc);
}

void PTPathVizPass::CreatePathVertexCountBuffer()
{
    nvrhi::BufferDesc vertexCountBufferDesc = {};
    vertexCountBufferDesc.byteSize = m_maxPaths * sizeof(PTPathSet);
    vertexCountBufferDesc.format = nvrhi::Format::UNKNOWN;
    vertexCountBufferDesc.canHaveTypedViews = true;
    vertexCountBufferDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
    vertexCountBufferDesc.debugName = "PTPathVizPathSetBuffer";
    vertexCountBufferDesc.canHaveUAVs = true;
    vertexCountBufferDesc.keepInitialState = true;
    vertexCountBufferDesc.structStride = sizeof(PTPathSet);
    m_pathSetBuffer = m_device->createBuffer(vertexCountBufferDesc);
}

void PTPathVizPass::CreateRenderVertexBuffers()
{
    nvrhi::BufferDesc lineVertexBufferDesc = {};
    lineVertexBufferDesc.byteSize = 2 * 2 * sizeof(float4) * m_maxPathLength * m_maxPaths;
    lineVertexBufferDesc.format = nvrhi::Format::RGBA32_FLOAT;
    lineVertexBufferDesc.canHaveTypedViews = true;
    lineVertexBufferDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
    lineVertexBufferDesc.debugName = "DebugPTLineVertexBuffer";
    lineVertexBufferDesc.canHaveUAVs = true;
    lineVertexBufferDesc.keepInitialState = true;
    lineVertexBufferDesc.isVertexBuffer = true;

    m_renderVertexBuffer = m_device->createBuffer(lineVertexBufferDesc);
    lineVertexBufferDesc.debugName = "DebugPTLineNormalBuffer";
    m_renderNormalBuffer = m_device->createBuffer(lineVertexBufferDesc);
    lineVertexBufferDesc.debugName = "DebugPTLineeNEEBuffer";
    m_renderNEELightBuffer = m_device->createBuffer(lineVertexBufferDesc);
}
