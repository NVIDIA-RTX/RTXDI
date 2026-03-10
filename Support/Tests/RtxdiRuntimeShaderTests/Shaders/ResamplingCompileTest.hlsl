/***************************************************************************
 # Copyright (c) 2020-2023, NVIDIA CORPORATION.  All rights reserved.
 #
 # NVIDIA CORPORATION and its licensors retain all intellectual property
 # and proprietary rights in and to this software, related documentation
 # and any modifications thereto.  Any use, reproduction, disclosure or
 # distribution of this software and related documentation without an express
 # license agreement from NVIDIA CORPORATION is strictly prohibited.
 **************************************************************************/

// This shader file is intended for testing the RTXDI header files to make sure
// that they do not make any undeclared assumptions about the contents of the 
// user-defined structures and about the functions being available.

#include <Rtxdi/DI/ReSTIRDIParameters.h>
#include <Rtxdi/GI/ReSTIRGIParameters.h>
#include <Rtxdi/PT/ReSTIRPTParameters.h>

#include "RtxdiApplicationBridge/RtxdiApplicationBridge.hlsli"
#define RTXDI_RESTIR_PT_INITIAL_SAMPLING
#include "RtxdiApplicationBridge/PathTracer/RAB_PathTracer.hlsli"

#define RTXDI_ENABLE_BOILING_FILTER
#define RTXDI_BOILING_FILTER_GROUP_SIZE 16

#define RTXDI_RIS_BUFFER u_RisBuffer
#define RTXDI_LIGHT_RESERVOIR_BUFFER u_LightReservoirs
#define RTXDI_GI_RESERVOIR_BUFFER u_GIReservoirs
#define RTXDI_NEIGHBOR_OFFSETS_BUFFER t_NeighborOffsets

#include <Rtxdi/LightSampling/PresamplingFunctions.hlsli>
#include <Rtxdi/DI/InitialSampling.hlsli>
#include <Rtxdi/DI/SpatialResampling.hlsli>
#include <Rtxdi/DI/SpatioTemporalResampling.hlsli>
#include <Rtxdi/DI/TemporalResampling.hlsli>
#include <Rtxdi/PT/InitialSampling.hlsli>
#include <Rtxdi/PT/TemporalResampling.hlsli>
#include <Rtxdi/PT/SpatialResampling.hlsli>

[numthreads(1, 1, 1)]
void main()
{
}
