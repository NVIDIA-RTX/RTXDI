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

#pragma once

#include <donut/engine/Scene.h>
#include <donut/engine/KeyframeAnimation.h>

namespace donut::engine
{
    class ThreadPool;
}

class SampleSceneTypeFactory : public donut::engine::SceneTypeFactory
{
public:
    std::shared_ptr<donut::engine::SceneGraphLeaf> CreateLeaf(const std::string& type) override;
    std::shared_ptr<donut::engine::MeshInfo> CreateMesh() override;
};

class SampleScene : public donut::engine::Scene
{
public:
    using Scene::Scene;

    bool LoadWithThreadPool(const std::filesystem::path& jsonFileName, donut::engine::ThreadPool* threadPool) override;

    const donut::engine::SceneGraphAnimation* GetBenchmarkAnimation() const;
    const donut::engine::PerspectiveCamera* GetBenchmarkCamera() const;

    void BuildMeshBLASes(nvrhi::IDevice* device);
    void UpdateSkinnedMeshBLASes(nvrhi::ICommandList* commandList, uint32_t frameIndex);
    void BuildTopLevelAccelStruct(nvrhi::ICommandList* commandList);
    void NextFrame();
    void Animate(float  fElapsedTimeSeconds);

    nvrhi::rt::IAccelStruct* GetTopLevelAS() const;
    nvrhi::rt::IAccelStruct* GetPrevTopLevelAS() const;

    std::vector<std::string>& GetEnvironmentMaps();

private:
    nvrhi::rt::AccelStructHandle m_topLevelAS;
    nvrhi::rt::AccelStructHandle m_prevTopLevelAS;
    std::vector<nvrhi::rt::InstanceDesc> m_tlasInstances;
    std::shared_ptr<donut::engine::SceneGraphAnimation> m_benchmarkAnimation;
    std::shared_ptr<donut::engine::PerspectiveCamera> m_benchmarkCamera;

    bool m_canUpdateTLAS = false;
    bool m_canUpdatePrevTLAS = false;

    double m_wallclockTime = 0;

    std::vector<std::string> m_environmentMaps;
};
