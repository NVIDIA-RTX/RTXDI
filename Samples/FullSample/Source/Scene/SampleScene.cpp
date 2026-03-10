/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#include "SampleScene.h"
#include "Lights.h"
#include "SampleMesh.h"
#include <donut/core/json.h>
#include <donut/core/vfs/VFS.h>
#include <donut/engine/ThreadPool.h>
#include <nvrhi/utils.h>
#include <nvrhi/common/misc.h>

#include "donut/engine/TextureCache.h"

using namespace donut;
using namespace donut::math;

#include "SharedShaderInclude/ShaderParameters.h"

std::shared_ptr<donut::engine::SceneGraphLeaf> SampleSceneTypeFactory::CreateLeaf(const std::string& type)
{
    if (type == "SpotLight")
    {
        return std::make_shared<SpotLightWithProfile>();
    }
    if (type == "EnvironmentLight")
    {
        return std::make_shared<EnvironmentLight>();
    }
    if (type == "CylinderLight")
    {
        return std::make_shared<CylinderLight>();
    }
    if (type == "DiskLight")
    {
        return std::make_shared<DiskLight>();
    }
    if (type == "RectLight")
    {
        return std::make_shared<RectLight>();
    }

    return SceneTypeFactory::CreateLeaf(type);
}

std::shared_ptr<donut::engine::MeshInfo> SampleSceneTypeFactory::CreateMesh()
{
    return std::make_shared<SampleMesh>();
}

bool SampleScene::LoadWithThreadPool(const std::filesystem::path& jsonFileName, donut::engine::ThreadPool* threadPool)
{
    if (!Scene::LoadWithThreadPool(jsonFileName, threadPool))
        return false;

    for (const auto& animation : GetSceneGraph()->GetAnimations())
    {
        if (animation->GetName() == "Benchmark")
        {
            m_benchmarkAnimation = animation;
            for (const auto& channel : animation->GetChannels())
            {
                const auto& targetNode = channel->GetTargetNode();
                if (targetNode)
                {
                    const auto& camera = std::dynamic_pointer_cast<engine::PerspectiveCamera>(targetNode->GetLeaf());
                    if (camera)
                    {
                        m_benchmarkCamera = camera;
                        break;
                    }
                }
            }
            break;
        }
    }

    // Enumerate the available environment maps
    std::vector<std::string> environmentMapNames;
    const std::string texturePath = "/media/environment/";
    m_fs->enumerateFiles(texturePath, { ".exr" }, donut::vfs::enumerate_to_vector(environmentMapNames));

    m_environmentMaps.clear();
    m_environmentMaps.push_back(""); // Procedural env.map with no name

    for (const std::string& mapName : environmentMapNames)
    {
        m_environmentMaps.push_back(texturePath + mapName);
    }

    return true;
}

const donut::engine::SceneGraphAnimation* SampleScene::GetBenchmarkAnimation() const
{
    return m_benchmarkAnimation.get();
}

const donut::engine::PerspectiveCamera* SampleScene::GetBenchmarkCamera() const
{
    return m_benchmarkCamera.get();
}

inline uint64_t AdvanceHeapPtr(uint64_t& heapPtr, const nvrhi::MemoryRequirements& memReq)
{
    heapPtr = nvrhi::align(heapPtr, memReq.alignment);
    uint64_t current = heapPtr;

    heapPtr += memReq.size;

    return current;
}

void SampleScene::BuildMeshBLASes(nvrhi::IDevice* device)
{
    assert(device->queryFeatureSupport(nvrhi::Feature::VirtualResources));

    uint64_t heapSize = 0;

    for (const auto& mesh : GetSceneGraph()->GetMeshes())
    {
        if (mesh->buffers->hasAttribute(engine::VertexAttribute::JointWeights))
            continue;

        nvrhi::rt::AccelStructDesc blasDesc;
        blasDesc.isTopLevel = false;
        blasDesc.isVirtual = true;

        for (const auto& geometry : mesh->geometries)
        {
            nvrhi::rt::GeometryDesc geometryDesc;
            auto& triangles = geometryDesc.geometryData.triangles;
            triangles.indexBuffer = mesh->buffers->indexBuffer;
            triangles.indexOffset = (mesh->indexOffset + geometry->indexOffsetInMesh) * sizeof(uint32_t);
            triangles.indexFormat = nvrhi::Format::R32_UINT;
            triangles.indexCount = geometry->numIndices;
            triangles.vertexBuffer = mesh->buffers->vertexBuffer;
            triangles.vertexOffset = (mesh->vertexOffset + geometry->vertexOffsetInMesh) * sizeof(float3) + mesh->buffers->getVertexBufferRange(engine::VertexAttribute::Position).byteOffset;
            triangles.vertexFormat = nvrhi::Format::RGB32_FLOAT;
            triangles.vertexStride = sizeof(float3);
            triangles.vertexCount = geometry->numVertices;
            geometryDesc.geometryType = nvrhi::rt::GeometryType::Triangles;
            geometryDesc.flags = (geometry->material->domain == engine::MaterialDomain::Opaque)
                ? nvrhi::rt::GeometryFlags::Opaque
                : nvrhi::rt::GeometryFlags::None;
            blasDesc.bottomLevelGeometries.push_back(geometryDesc);
        }

        blasDesc.buildFlags = nvrhi::rt::AccelStructBuildFlags::PreferFastTrace;
        if (!mesh->skinPrototype)
        {
            // Only allow compaction on non-skinned, static meshes.
            blasDesc.buildFlags = blasDesc.buildFlags | nvrhi::rt::AccelStructBuildFlags::AllowCompaction;
        }

        blasDesc.trackLiveness = false;
        blasDesc.debugName = mesh->name;

        nvrhi::rt::AccelStructHandle as = device->createAccelStruct(blasDesc);

        AdvanceHeapPtr(heapSize, device->getAccelStructMemoryRequirements(as));

        // If this is a skinned mesh, create a second BLAS to toggle with the first one on every frame.
        // RTXDI needs access to the previous frame geometry in order to be unbiased.
        if (mesh->skinPrototype)
        {
            auto sampleMesh = dynamic_cast<SampleMesh*>(mesh.get());
            assert(sampleMesh);
            sampleMesh->prevAccelStruct = device->createAccelStruct(blasDesc);
            AdvanceHeapPtr(heapSize, device->getAccelStructMemoryRequirements(as));
        }

        mesh->accelStruct = as;
    }

    nvrhi::rt::AccelStructDesc tlasDesc;
    tlasDesc.isTopLevel = true;
    tlasDesc.isVirtual = true;
    tlasDesc.topLevelMaxInstances = GetSceneGraph()->GetMeshInstances().size();
    tlasDesc.debugName = "TopLevelAS";
    tlasDesc.buildFlags = nvrhi::rt::AccelStructBuildFlags::AllowUpdate;

    m_topLevelAS = device->createAccelStruct(tlasDesc);

    AdvanceHeapPtr(heapSize, device->getAccelStructMemoryRequirements(m_topLevelAS));

    tlasDesc.debugName = "PrevTopLevelAS";

    m_prevTopLevelAS = device->createAccelStruct(tlasDesc);

    AdvanceHeapPtr(heapSize, device->getAccelStructMemoryRequirements(m_prevTopLevelAS));


    nvrhi::HeapDesc heapDecs;
    heapDecs.type = nvrhi::HeapType::DeviceLocal;
    heapDecs.capacity = heapSize;
    heapDecs.debugName = "AccelStructHeap";

    nvrhi::HeapHandle heap = device->createHeap(heapDecs);

    heapSize = 0;

    for (const auto& mesh : GetSceneGraph()->GetMeshes())
    {
        if (!mesh->accelStruct)
            continue;

        uint64_t heapOffset = AdvanceHeapPtr(heapSize, device->getAccelStructMemoryRequirements(mesh->accelStruct));

        device->bindAccelStructMemory(mesh->accelStruct, heap, heapOffset);

        // Bind memory for the second BLAS for skinned meshes.
        if (mesh->skinPrototype)
        {
            auto sampleMesh = dynamic_cast<SampleMesh*>(mesh.get());
            assert(sampleMesh);

            heapOffset = AdvanceHeapPtr(heapSize, device->getAccelStructMemoryRequirements(sampleMesh->prevAccelStruct));
            device->bindAccelStructMemory(sampleMesh->prevAccelStruct, heap, heapOffset);
        }
    }

    uint64_t heapOffset = AdvanceHeapPtr(heapSize, device->getAccelStructMemoryRequirements(m_topLevelAS));

    device->bindAccelStructMemory(m_topLevelAS, heap, heapOffset);

    heapOffset = AdvanceHeapPtr(heapSize, device->getAccelStructMemoryRequirements(m_prevTopLevelAS));

    device->bindAccelStructMemory(m_prevTopLevelAS, heap, heapOffset);


    nvrhi::CommandListParameters clparams;
    clparams.scratchChunkSize = clparams.scratchMaxMemory;

    nvrhi::CommandListHandle commandList = device->createCommandList(clparams);
    commandList->open();

    for (const auto& mesh : GetSceneGraph()->GetMeshes())
    {
        if (!mesh->accelStruct)
            continue;

        // Get the desc from the AS, restore the buffer pointers because they're erased by nvrhi
        nvrhi::rt::AccelStructDesc blasDesc = mesh->accelStruct->getDesc();
        for (auto& geometryDesc : blasDesc.bottomLevelGeometries)
        {
            geometryDesc.geometryData.triangles.indexBuffer = mesh->buffers->indexBuffer;
            geometryDesc.geometryData.triangles.vertexBuffer = mesh->buffers->vertexBuffer;
        }

        nvrhi::utils::BuildBottomLevelAccelStruct(commandList, mesh->accelStruct, blasDesc);
    }

    commandList->close();
    device->executeCommandList(commandList);

    device->waitForIdle();
    device->runGarbageCollection();
}

void SampleScene::UpdateSkinnedMeshBLASes(nvrhi::ICommandList* commandList, uint32_t frameIndex)
{
    commandList->beginMarker("Skinned BLAS Updates");

    // Transition all the buffers to their necessary states before building the BLAS'es to allow BLAS batching
    for (const auto& skinnedInstance : GetSceneGraph()->GetSkinnedMeshInstances())
    {
        if (skinnedInstance->GetLastUpdateFrameIndex() < frameIndex)
            continue;

        auto sampleMesh = dynamic_cast<SampleMesh*>(skinnedInstance->GetMesh().get());
        assert(sampleMesh);
        assert(sampleMesh->prevAccelStruct);
        std::swap(sampleMesh->accelStruct, sampleMesh->prevAccelStruct);

        commandList->setAccelStructState(skinnedInstance->GetMesh()->accelStruct, nvrhi::ResourceStates::AccelStructWrite);
        commandList->setBufferState(skinnedInstance->GetMesh()->buffers->vertexBuffer, nvrhi::ResourceStates::AccelStructBuildInput);
    }
    commandList->commitBarriers();

    // Now build the BLAS'es
    for (const auto& skinnedInstance : GetSceneGraph()->GetSkinnedMeshInstances())
    {
        if (skinnedInstance->GetLastUpdateFrameIndex() < frameIndex)
            continue;

        const auto& mesh = skinnedInstance->GetMesh();
        nvrhi::rt::AccelStructDesc blasDesc = mesh->accelStruct->getDesc();
        for (auto& geometryDesc : blasDesc.bottomLevelGeometries)
        {
            geometryDesc.geometryData.triangles.indexBuffer = mesh->buffers->indexBuffer;
            geometryDesc.geometryData.triangles.vertexBuffer = mesh->buffers->vertexBuffer;
        }

        nvrhi::utils::BuildBottomLevelAccelStruct(commandList, mesh->accelStruct, blasDesc);
    }
    commandList->endMarker();
}

void SampleScene::BuildTopLevelAccelStruct(nvrhi::ICommandList* commandList)
{
    m_tlasInstances.resize(GetSceneGraph()->GetMeshInstances().size());

    nvrhi::rt::AccelStructBuildFlags buildFlags = m_canUpdateTLAS
        ? nvrhi::rt::AccelStructBuildFlags::PerformUpdate
        : nvrhi::rt::AccelStructBuildFlags::None;

    uint32_t index = 0;

    for (const auto& instance : GetSceneGraph()->GetMeshInstances())
    {
        const auto& mesh = instance->GetMesh();

        if (!mesh->accelStruct)
            continue;

        nvrhi::rt::InstanceDesc& instanceDesc = m_tlasInstances[index++];

        instanceDesc.instanceMask = 0;
        engine::SceneContentFlags contentFlags = instance->GetContentFlags();

        if ((contentFlags & engine::SceneContentFlags::OpaqueMeshes) != 0)
            instanceDesc.instanceMask |= INSTANCE_MASK_OPAQUE;

        if ((contentFlags & engine::SceneContentFlags::AlphaTestedMeshes) != 0)
            instanceDesc.instanceMask |= INSTANCE_MASK_ALPHA_TESTED;

        if ((contentFlags & engine::SceneContentFlags::BlendedMeshes) != 0)
            instanceDesc.instanceMask |= INSTANCE_MASK_TRANSPARENT;

        for (const auto& geometry : mesh->geometries)
        {
            if (geometry->material->doubleSided)
                instanceDesc.flags = nvrhi::rt::InstanceFlags::TriangleCullDisable;
        }

        instanceDesc.bottomLevelAS = mesh->accelStruct;

        auto node = instance->GetNode();
        if (node)
            dm::affineToColumnMajor(node->GetLocalToWorldTransformFloat(), instanceDesc.transform);

        instanceDesc.instanceID = uint(instance->GetInstanceIndex());
    }

    commandList->buildTopLevelAccelStruct(m_topLevelAS, m_tlasInstances.data(), m_tlasInstances.size(), buildFlags);
    m_canUpdateTLAS = true;
}

void SampleScene::NextFrame()
{
    std::swap(m_topLevelAS, m_prevTopLevelAS);
    std::swap(m_canUpdateTLAS, m_canUpdatePrevTLAS);
}

void SampleScene::Animate(float fElapsedTimeSeconds)
{
    m_wallclockTime += fElapsedTimeSeconds;

    for (const auto& animation : m_SceneGraph->GetAnimations())
    {
        if (animation == m_benchmarkAnimation)
            continue;

        float duration = animation->GetDuration();
        double integral;
        float animationTime = float(std::modf(m_wallclockTime / double(duration), &integral)) * duration;
        (void)animation->Apply(animationTime);
    }
}

nvrhi::rt::IAccelStruct* SampleScene::GetTopLevelAS() const
{
    return m_topLevelAS;
}

nvrhi::rt::IAccelStruct* SampleScene::GetPrevTopLevelAS() const
{
    return m_prevTopLevelAS;
}

std::vector<std::string>& SampleScene::GetEnvironmentMaps()
{
    return m_environmentMaps;
}
