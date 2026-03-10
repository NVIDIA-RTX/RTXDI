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

#ifndef PT_PATH_VIZ_PREPROCESS_PASS_H
#define PT_PATH_VIZ_PREPROCESS_PASS_H

#include <memory>

#include <nvrhi/nvrhi.h>

namespace donut::engine
{
    class Scene;
    class ShaderFactory;
}

class PTPathVizPreprocessPass
{
public:
	PTPathVizPreprocessPass(nvrhi::IDevice* device,
                            std::shared_ptr<donut::engine::ShaderFactory> shaderFactory);

    void CreateBindingSet(nvrhi::BufferHandle pathVertexRecord,
                          nvrhi::BufferHandle pathVertexCount,
                          nvrhi::BufferHandle vertexLineList,
                          nvrhi::BufferHandle normalLineList,
                          nvrhi::BufferHandle neeLightLineList);

    void CreatePipeline();

    void ConvertPathRecordToLineListBuffers(nvrhi::ICommandList* commandList, uint32_t maxPaths, uint32_t maxVerticesPerPath);

    void SetNormalVectorLength(float length);
    float GetNormalVectorLength() const;

private:
    void CreateBindingLayout();
    void CreateConstantBuffer();

    void UpdateConstantBuffer(nvrhi::ICommandList* commandList, uint32_t maxVerticesPerPath);
    void SetComputeState(nvrhi::ICommandList* commandList);
    void PreprocessLines(nvrhi::ICommandList* commandList, uint32_t maxPaths);

    nvrhi::DeviceHandle m_device;

    std::shared_ptr<donut::engine::ShaderFactory> m_shaderFactory;

    nvrhi::BindingLayoutHandle m_bindingLayout;
    nvrhi::BindingSetHandle m_bindingSet;
    nvrhi::ComputePipelineHandle m_pipeline;

    nvrhi::BufferHandle m_constantBuffer;

    float m_normalVectorVizLength;
    uint32_t m_maxVerticesPerPath;
};

#endif // PT_PATH_VIZ_PREPROCESS_PASS_H