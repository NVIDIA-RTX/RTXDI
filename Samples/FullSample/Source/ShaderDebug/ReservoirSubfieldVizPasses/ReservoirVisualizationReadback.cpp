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

#include "ReservoirVisualizationReadback.h"
#include <cstring>

ReservoirVisualizationReadback::ReservoirVisualizationReadback(nvrhi::IDevice* device)
    : m_device(device)
{
    nvrhi::BufferDesc counterDesc;
    counterDesc.byteSize = sizeof(uint32_t) * RESERVOIR_VIZ_STATUS_COUNT;
    counterDesc.structStride = sizeof(uint32_t);
    counterDesc.canHaveUAVs = true;
    counterDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
    counterDesc.keepInitialState = true;
    counterDesc.debugName = "ReservoirVisualizationCounts";
    m_counterBuffer = m_device->createBuffer(counterDesc);

    nvrhi::BufferDesc readbackDesc;
    readbackDesc.byteSize = counterDesc.byteSize;
    readbackDesc.cpuAccess = nvrhi::CpuAccessMode::Read;
    readbackDesc.initialState = nvrhi::ResourceStates::CopyDest;
    readbackDesc.keepInitialState = true;
    readbackDesc.debugName = "ReservoirVisualizationCountReadback";
    for (auto& slot : m_slots)
    {
        slot.buffer = m_device->createBuffer(readbackDesc);
        slot.query = m_device->createEventQuery();
    }
}

nvrhi::IBuffer* ReservoirVisualizationReadback::GetCounterBuffer() const
{
    return m_counterBuffer;
}

const ReservoirVisualizationStatistics& ReservoirVisualizationReadback::GetStatistics() const
{
    return m_latest;
}

void ReservoirVisualizationReadback::Invalidate()
{
    ++m_generation;
    m_hasRequest = false;
    m_latest = {};
}

void ReservoirVisualizationReadback::PollCompleted()
{
    for (auto& slot : m_slots)
    {
        if (!slot.submitted || !m_device->pollEventQuery(slot.query))
        {
            continue;
        }
        if (slot.generation == m_generation && slot.sequence > m_latestSequence)
        {
            const void* data = m_device->mapBuffer(slot.buffer, nvrhi::CpuAccessMode::Read);
            if (data)
            {
                std::memcpy(slot.snapshot.counts.data(), data, sizeof(slot.snapshot.counts));
                m_device->unmapBuffer(slot.buffer);
                m_latest = slot.snapshot;
                m_latest.available = true;
                m_latestSequence = slot.sequence;
            }
        }
        m_device->resetEventQuery(slot.query);
        slot.pending = false;
        slot.submitted = false;
    }
}

void ReservoirVisualizationReadback::BeginFrame(nvrhi::ICommandList* commandList, uint32_t fieldIndex,
    const ReservoirVisualizationParameters& parameters, uint32_t frameIndex, uint32_t width, uint32_t height)
{
    if (!m_hasRequest || m_request.fieldIndex != fieldIndex || m_request.width != width || m_request.height != height
        || !ReservoirVisualizationParametersMatch(m_request.parameters, parameters))
    {
        Invalidate();
    }
    m_request = {};
    m_request.fieldIndex = fieldIndex;
    m_request.parameters = parameters;
    m_request.frameIndex = frameIndex;
    m_request.width = width;
    m_request.height = height;
    m_hasRequest = true;
    PollCompleted();
    commandList->clearBufferUInt(m_counterBuffer, 0);
}

void ReservoirVisualizationReadback::Capture(nvrhi::ICommandList* commandList)
{
    for (auto& slot : m_slots)
    {
        if (slot.pending)
        {
            continue;
        }
        commandList->copyBuffer(slot.buffer, 0, m_counterBuffer, 0, m_counterBuffer->getDesc().byteSize);
        slot.snapshot = m_request;
        slot.generation = m_generation;
        slot.sequence = ++m_sequence;
        slot.pending = true;
        return;
    }
    // A busy GPU can fill the ring. Keep the last completed counts instead of waiting.
}

void ReservoirVisualizationReadback::NotifySubmitted()
{
    for (auto& slot : m_slots)
    {
        if (slot.pending && !slot.submitted)
        {
            m_device->setEventQuery(slot.query, nvrhi::CommandQueue::Graphics);
            slot.submitted = true;
        }
    }
}
