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

#ifndef PT_PATH_VIZ_RECORDING_HLSLI
#define PT_PATH_VIZ_RECORDING_HLSLI

/*
 *  Path tracing visualization tool for debugging ReSTIR PT
 *
 *  Each path consists of a sequence of "vertex records"
 *  A record contains a surface location, a normal vector, and
 *  an NEE light location.
 *  To save on memory traffic, paths without NEE samples must
 *  manually set their NEE locations to the surface location
 *  to avoid drawing lines to the origin
 *
 *  Each path belongs to one of 7 categories:
 *  - Initial path
 *  - Temporal retrace
 *  - Temporal shift
 *  - Temporal inverse shift
 *  - Spatial retrace
 *  - Spatial shift
 *  - Spatial inverse shift
 *
 *  Call the corresponding Begin function before path tracing
 *  to keep the paths separate. Failure to do so will result in
 *  subsequently recorded paths overwriting earlier ones.
 *
 */

#ifndef DEBUG_PT_VERTEX_RECORD_BUFFER
#error "PT Path Visualization requires DEBUG_PT_VERTEX_RECORD_BUFFER to be defined to point to a RWStructuredBuffer of PTVertexRecord"
#endif

#ifndef DEBUG_PT_PATH_SET_BUFFER
#error "PT Path Visualization requires DEBUG_PT_PATH_SET_BUFFER to be defined to point to a RWStructuredBuffer of PTPathSet"
#endif

static uint g_DebugPTPathRecordEnabled = 0;

#if PT_PATH_VIZ_ENABLED

// Index of the path being recorded
static uint g_DebugPTPathIndex = 1;

// Index of the first vertex of the path being recorded in the global vertex buffer
static uint g_DebugPTPathRecordVertexIndex = 0;

// Offset from the first vertex of the path be
static uint g_DebugPTPathRecordVertexStartIndex = 2;

#endif

void Debug_EnablePTPathRecording()
{
#if PT_PATH_VIZ_ENABLED
	g_DebugPTPathRecordEnabled = 1;
#endif
}

void Debug_DisablePTPathRecording()
{
#if PT_PATH_VIZ_ENABLED
	g_DebugPTPathRecordEnabled = 0;
#endif
}

// Call before the first path is recorded in a frame
// Can be called from the right PT shader or from a dedicated
//     shader to ensure correctness. Calling before the
//     initial path tracer should be fine, though.
void Debug_CleanPTPathRecord()
{
#if PT_PATH_VIZ_ENABLED
    DEBUG_PT_PATH_SET_BUFFER[0].vertexBeginIndex = 0;
    DEBUG_PT_PATH_SET_BUFFER[0].vertexEndIndex = 0;
#endif
}

void Debug_RecordPTFinalVertexCount(uint vertexCount);

void Debug_BeginPath(RTXDI_PTPathTraceInvocationType type)
{
#if PT_PATH_VIZ_ENABLED
    if(g_DebugPTPathRecordEnabled)
    {
        // Safeguard - detect when Debug_RecordPTFinalVertexCount is not called
        if(DEBUG_PT_PATH_SET_BUFFER[g_DebugPTPathIndex].vertexBeginIndex > 0 &&
           DEBUG_PT_PATH_SET_BUFFER[g_DebugPTPathIndex].vertexBeginIndex == DEBUG_PT_PATH_SET_BUFFER[g_DebugPTPathIndex].vertexEndIndex)
        {
            Debug_RecordPTFinalVertexCount(0);
        }

        // Increase the total number of paths in the special entry
        DEBUG_PT_PATH_SET_BUFFER[0].vertexEndIndex++;

        // Cache the path index
        g_DebugPTPathIndex = DEBUG_PT_PATH_SET_BUFFER[0].vertexEndIndex;
    
        // Start the paths at the next free slot in the buffer by reading it from the special entry.
        DEBUG_PT_PATH_SET_BUFFER[g_DebugPTPathIndex].vertexBeginIndex = DEBUG_PT_PATH_SET_BUFFER[0].vertexBeginIndex;

        // Cache the starting index of the path in the global vertex buffer
        g_DebugPTPathRecordVertexStartIndex = DEBUG_PT_PATH_SET_BUFFER[g_DebugPTPathIndex].vertexBeginIndex;

        // Set the end index to the begin index to safeguard against the case when
        //     RecordPTFinalVertexCount is not called.
        DEBUG_PT_PATH_SET_BUFFER[g_DebugPTPathIndex].vertexEndIndex = g_DebugPTPathRecordVertexStartIndex;

        // Start recording at the secondary hit.
        g_DebugPTPathRecordVertexIndex = g_DebugPTPathRecordVertexStartIndex + 2;

        DEBUG_PT_PATH_SET_BUFFER[g_DebugPTPathIndex].type = type;
    }
#endif
}

void Debug_SetPTVertexIndex(uint index)
{
#if PT_PATH_VIZ_ENABLED
	if(g_DebugPTPathRecordEnabled)
	{
		g_DebugPTPathRecordVertexIndex = g_DebugPTPathRecordVertexStartIndex + index;
	}
#endif
}

void Debug_RecordPTCameraPosition(float3 cameraPosition)
{
#if PT_PATH_VIZ_ENABLED
	if(g_DebugPTPathRecordEnabled)
	{
		DEBUG_PT_VERTEX_RECORD_BUFFER[g_DebugPTPathRecordVertexStartIndex].position = cameraPosition;
	}
#endif
}

void Debug_RecordPTIntersectionPosition(float3 intersectionPosition)
{
#if PT_PATH_VIZ_ENABLED
	if(g_DebugPTPathRecordEnabled)
	{
		DEBUG_PT_VERTEX_RECORD_BUFFER[g_DebugPTPathRecordVertexIndex].position = intersectionPosition;
        DEBUG_PT_VERTEX_RECORD_BUFFER[g_DebugPTPathRecordVertexIndex].neeLightPosition = intersectionPosition;
	}
#endif
}

void Debug_RecordPTIntersectionNormal(float3 normal)
{
#if PT_PATH_VIZ_ENABLED
	if(g_DebugPTPathRecordEnabled)
	{
		DEBUG_PT_VERTEX_RECORD_BUFFER[g_DebugPTPathRecordVertexIndex].normal = normal;
	}
#endif
}

void Debug_RecordPTNEELightPosition(float3 neeLightPosition)
{
#if PT_PATH_VIZ_ENABLED
	if(g_DebugPTPathRecordEnabled)
	{
		DEBUG_PT_VERTEX_RECORD_BUFFER[g_DebugPTPathRecordVertexIndex].neeLightPosition = neeLightPosition;
	}
#endif
}

void Debug_RecordPTLastNEELightPositionForPriorPath(float3 neeLightPosition)
{
#if PT_PATH_VIZ_ENABLED
	if(g_DebugPTPathRecordEnabled)
	{
		DEBUG_PT_VERTEX_RECORD_BUFFER[g_DebugPTPathRecordVertexIndex-1].neeLightPosition = neeLightPosition;
	}
#endif
}

void Debug_RecordPTFinalVertexCount(uint vertexCount)
{
#if PT_PATH_VIZ_ENABLED
	if(g_DebugPTPathRecordEnabled)
	{
        // Set the end of the interval containing the vertices
        DEBUG_PT_PATH_SET_BUFFER[g_DebugPTPathIndex].vertexEndIndex = g_DebugPTPathRecordVertexStartIndex + vertexCount;

        // Update the special record with the location of the next available path entry.
        DEBUG_PT_PATH_SET_BUFFER[0].vertexBeginIndex = DEBUG_PT_PATH_SET_BUFFER[g_DebugPTPathIndex].vertexEndIndex + 2;
	}
#endif
}

#endif // PT_PATH_VIZ_RECORDING_HLSLI
