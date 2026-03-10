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

#ifndef RAB_PATH_TRACER_USER_DATA_HLSLI
#define RAB_PATH_TRACER_USER_DATA_HLSLI

#include "../../PT/PSRUtils.hlsli"

//
// Struct for sending data back from internal invocations of RAB_PathTrace
// back to the user code.
//
struct RAB_PathTracerUserData
{
    PSRData psr;

    RTXDI_PTPathTraceInvocationType pathType;
};

RAB_PathTracerUserData RAB_EmptyPathTracerUserData()
{
    RAB_PathTracerUserData ptud = (RAB_PathTracerUserData)0;
    ptud.psr = GetEmptyPSRData();
    return ptud;
}

void RAB_PathTracerUserDataSetPathType(inout RAB_PathTracerUserData ptud, RTXDI_PTPathTraceInvocationType type)
{
    ptud.pathType = type;
}

#endif // RAB_PATH_TRACER_USER_DATA_HLSLI
