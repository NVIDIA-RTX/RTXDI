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

#include "PTPathVizPreprocessPass.h"

#include <nvrhi/utils.h>

#include <donut/core/log.h>
#include <donut/core/math/math.h>
#include <donut/engine/ShaderFactory.h>

using namespace donut::math;
#include <SharedShaderInclude/ShaderDebug/PTPathViz/PTPathVizConstants.h>

PTPathVizPreprocessPass::PTPathVizPreprocessPass(nvrhi::IDevice* device,
                                                 std::shared_ptr<donut::engine::ShaderFactory> shaderFactory) :
    m_device(device),
    m_shaderFactory(shaderFactory),
    m_normalVectorVizLength(1.0f)
{
    CreateBindingLayout();
    CreateConstantBuffer();
}

void PTPathVizPreprocessPass::CreateBindingSet(nvrhi::BufferHandle pathVertexRecord,
                                               nvrhi::BufferHandle pathSetRecord,
                                               nvrhi::BufferHandle vertexLineList,
                                               nvrhi::BufferHandle normalLineList,
                                               nvrhi::BufferHandle neeLightLineList)
{
    nvrhi::BindingSetDesc bindingSetDesc;
    bindingSetDesc.bindings =
    {
        nvrhi::BindingSetItem::ConstantBuffer(0, m_constantBuffer),
        nvrhi::BindingSetItem::StructuredBuffer_SRV(0, pathVertexRecord),
        nvrhi::BindingSetItem::StructuredBuffer_SRV(1, pathSetRecord),
        nvrhi::BindingSetItem::TypedBuffer_UAV(0, vertexLineList),
        nvrhi::BindingSetItem::TypedBuffer_UAV(1, normalLineList),
        nvrhi::BindingSetItem::TypedBuffer_UAV(2, neeLightLineList)
    };

    m_bindingSet = m_device->createBindingSet(bindingSetDesc, m_bindingLayout);
}

void PTPathVizPreprocessPass::CreatePipeline()
{
    donut::log::debug("Initializing PTPathVizPreprocessPass...");

    using namespace donut::engine;

    nvrhi::ComputePipelineDesc pipelineDesc;

    pipelineDesc.bindingLayouts = { m_bindingLayout };
    pipelineDesc.CS = m_shaderFactory->CreateShader("app/ShaderDebug/PTPathViz/PTPathVizPreprocess_cs.hlsl", "main", nullptr, nvrhi::ShaderType::Compute);

    m_pipeline = m_device->createComputePipeline(pipelineDesc);
}

void PTPathVizPreprocessPass::ConvertPathRecordToLineListBuffers(nvrhi::ICommandList* commandList, uint32_t maxPaths, uint32_t maxVerticesPerPath)
{
    commandList->beginMarker("PTPathVizPreprocessPass");

    //commandList->setEnableAutomaticBarriers(false);
    //commandList->commitBarriers();
    UpdateConstantBuffer(commandList, maxVerticesPerPath);
    SetComputeState(commandList);
    PreprocessLines(commandList, maxPaths);

    //commandList->setEnableAutomaticBarriers(true);
    commandList->endMarker();
}

void PTPathVizPreprocessPass::SetNormalVectorLength(float length)
{
    m_normalVectorVizLength = length;
}

float PTPathVizPreprocessPass::GetNormalVectorLength() const
{
    return m_normalVectorVizLength;
}

void PTPathVizPreprocessPass::CreateBindingLayout()
{
    nvrhi::BindingLayoutDesc globalBindingLayoutDesc;
    globalBindingLayoutDesc.visibility = nvrhi::ShaderType::Compute;
    globalBindingLayoutDesc.bindings =
    {
        nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
        nvrhi::BindingLayoutItem::StructuredBuffer_SRV(0),
        nvrhi::BindingLayoutItem::StructuredBuffer_SRV(1),
        nvrhi::BindingLayoutItem::TypedBuffer_UAV(0),
        nvrhi::BindingLayoutItem::TypedBuffer_UAV(1),
        nvrhi::BindingLayoutItem::TypedBuffer_UAV(2)
    };

    m_bindingLayout = m_device->createBindingLayout(globalBindingLayoutDesc);
}

void PTPathVizPreprocessPass::CreateConstantBuffer()
{
    nvrhi::BufferDesc cbDesc = {};
    cbDesc.byteSize = sizeof(PTPathVizPreprocessConstants);
    cbDesc.format = nvrhi::Format::UNKNOWN;
    cbDesc.canHaveTypedViews = false;
    cbDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
    cbDesc.debugName = "PTPathVizPreprocessConstants";
    cbDesc.canHaveUAVs = true;
    cbDesc.keepInitialState = true;
    cbDesc.structStride = sizeof(PTPathVizPreprocessConstants);

    m_constantBuffer = m_device->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(PTPathVizPreprocessConstants), "PTPathVizPreprocessConstants", 16));
}

void PTPathVizPreprocessPass::UpdateConstantBuffer(nvrhi::ICommandList* commandList, uint32_t maxVerticesPerPath)
{
    PTPathVizPreprocessConstants cbData = {};
    cbData.normalVectorVizLength = m_normalVectorVizLength;
    cbData.maxVerticesPerPath = maxVerticesPerPath;
    commandList->writeBuffer(m_constantBuffer, &cbData, sizeof(cbData));
}

void PTPathVizPreprocessPass::SetComputeState(nvrhi::ICommandList* commandList)
{
    nvrhi::ComputeState computeState = {};
    computeState.setPipeline(m_pipeline);
    computeState.addBindingSet(m_bindingSet);
    commandList->setComputeState(computeState);
}

void PTPathVizPreprocessPass::PreprocessLines(nvrhi::ICommandList* commandList, uint32_t maxPaths)
{
    commandList->dispatch(1, maxPaths, 1);
}