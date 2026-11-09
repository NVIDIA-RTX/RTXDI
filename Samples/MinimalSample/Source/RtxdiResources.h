/***************************************************************************
 # Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
 #
 # NVIDIA CORPORATION and its licensors retain all intellectual property
 # and proprietary rights in and to this software, related documentation
 # and any modifications thereto.  Any use, reproduction, disclosure or
 # distribution of this software and related documentation without an express
 # license agreement from NVIDIA CORPORATION is strictly prohibited.
 **************************************************************************/

#pragma once

#include <nvrhi/nvrhi.h>

enum class ReSTIRDI_SpatialSamplingMode : uint32_t;

namespace rtxdi
{
    class ReSTIRDIContext;
}

class RtxdiResources
{
public:
    nvrhi::BufferHandle TaskBuffer;
    nvrhi::BufferHandle LightDataBuffer;
    nvrhi::BufferHandle NeighborOffsetsBuffer;
    nvrhi::BufferHandle LightReservoirBuffer;
    nvrhi::BufferHandle GeometryInstanceToLightBuffer;

    RtxdiResources(
        nvrhi::IDevice* device, 
        const rtxdi::ReSTIRDIContext& context,
        uint32_t maxEmissiveMeshes,
        uint32_t maxEmissiveTriangles,
        uint32_t maxMeshInstances);

    void InitializeNeighborOffsets(
        nvrhi::ICommandList* commandList,
        uint32_t neighborOffsetCount,
        ReSTIRDI_SpatialSamplingMode samplingMode);

    uint32_t GetMaxEmissiveMeshes() const;
    uint32_t GetMaxEmissiveTriangles() const;
    uint32_t GetMaxGeometryInstances() const;

private:
    bool m_neighborOffsetsInitialized;
    ReSTIRDI_SpatialSamplingMode m_neighborOffsetSamplingMode = {};
    uint32_t m_maxEmissiveMeshes;
    uint32_t m_maxEmissiveTriangles;
    uint32_t m_maxGeometryInstances;
};
