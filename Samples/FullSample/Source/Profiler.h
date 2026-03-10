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

#include <nvrhi/nvrhi.h>
#include <array>
#include <memory>

#include "ProfilerSections.h"

class RenderTargets;

namespace donut::app
{
    class DeviceManager;
}

class Profiler
{
public:
    explicit Profiler(donut::app::DeviceManager& deviceManager);

    bool IsEnabled() const;
    void EnableProfiler(bool enable);
    void EnableAccumulation(bool enable);
    void ResetAccumulation();
    void ResolvePreviousFrame();
    void BeginFrame(nvrhi::ICommandList* commandList);
    void EndFrame(nvrhi::ICommandList* commandList);
    void BeginSection(nvrhi::ICommandList* commandList, ProfilerSection::Enum section);
    void EndSection(nvrhi::ICommandList* commandList, ProfilerSection::Enum section);
    void SetRenderTargets(const std::shared_ptr<RenderTargets>& renderTargets);

    double GetTimer(ProfilerSection::Enum section);
    double GetRayCount(ProfilerSection::Enum section);
    double GetHitCount(ProfilerSection::Enum section);
    int GetMaterialReadback();

    void BuildUI(bool enableRayCounts);
    std::string GetAsText();

    [[nodiscard]] nvrhi::IBuffer* GetRayCountBuffer() const;

private:
    bool m_enabled = true;
    bool m_isAccumulating = false;
    uint32_t m_accumulatedFrames = 0;
    uint32_t m_activeBank = 0;

    std::array<nvrhi::TimerQueryHandle, ProfilerSection::Count * 2> m_timerQueries;
    std::array<double, ProfilerSection::Count> m_timerValues{};
    std::array<size_t, ProfilerSection::Count> m_rayCounts{};
    std::array<size_t, ProfilerSection::Count> m_hitCounts{};
    std::array<bool, ProfilerSection::Count * 2> m_timersUsed{};

    donut::app::DeviceManager& m_deviceManager;
    nvrhi::DeviceHandle m_device;
    nvrhi::BufferHandle m_rayCountBuffer;
    std::array<nvrhi::BufferHandle, 2> m_rayCountReadback;
    std::weak_ptr<RenderTargets> m_renderTargets;
};

class ProfilerScope
{
public:
    ProfilerScope(Profiler& profiler, nvrhi::ICommandList* commandList, ProfilerSection::Enum section);
    ~ProfilerScope();

    // Non-copyable and non-movable
    ProfilerScope(const ProfilerScope&) = delete;
    ProfilerScope(const ProfilerScope&&) = delete;
    ProfilerScope& operator=(const ProfilerScope&) = delete;
    ProfilerScope& operator=(const ProfilerScope&&) = delete;

private:
    Profiler& m_Profiler;
    nvrhi::ICommandList* m_CommandList;
    ProfilerSection::Enum m_Section;
};
