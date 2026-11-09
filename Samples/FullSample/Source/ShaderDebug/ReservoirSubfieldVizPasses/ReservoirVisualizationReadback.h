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

#include <nvrhi/nvrhi.h>

#include "ReservoirVisualizationControls.h"

// A small asynchronous ring keeps diagnostic counts off the render thread's critical path.
class ReservoirVisualizationReadback
{
public:
    explicit ReservoirVisualizationReadback(nvrhi::IDevice* device);

    nvrhi::IBuffer* GetCounterBuffer() const;
    const ReservoirVisualizationStatistics& GetStatistics() const;

    void Invalidate();

    void BeginFrame(
        nvrhi::ICommandList* commandList,
        uint32_t fieldIndex,
        const ReservoirVisualizationParameters& parameters,
        uint32_t frameIndex,
        uint32_t width,
        uint32_t height);

    void Capture(nvrhi::ICommandList* commandList);

    // Call after executing the command list containing Capture, on the graphics queue.
    void NotifySubmitted();

private:
    struct Slot
    {
        nvrhi::BufferHandle buffer;
        nvrhi::EventQueryHandle query;

        ReservoirVisualizationStatistics snapshot;

        uint64_t generation = 0;
        uint64_t sequence = 0;
        bool pending = false;
        bool submitted = false;
    };

    void PollCompleted();

    nvrhi::DeviceHandle m_device;

    nvrhi::BufferHandle m_counterBuffer;
    std::array<Slot, 3> m_slots;

    ReservoirVisualizationStatistics m_request;
    ReservoirVisualizationStatistics m_latest;

    uint64_t m_generation = 0;
    uint64_t m_sequence = 0;
    uint64_t m_latestSequence = 0;
    bool m_hasRequest = false;
};
