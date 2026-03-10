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

#ifndef PT_PATH_SET_RECORD_H
#define PT_PATH_SET_RECORD_H

#define PT_PATH_VIZ_ENABLED 0

#include "Rtxdi/PT/ReSTIRPTParameters.h"

enum DEBUG_PATH_VIZ_LINE_TYPE
{
    PATH = 0,
    NORMAL = 1,
    NEE = 2
};

// const uint DEBUG_PATH_VIZ_MAX_SPATIAL_PATHS = 32;

/*
 *  Specifies what kind of path was recorded and
 * 
 *  The first entry in the PTPathSet buffer is a special case.
 *  - vertexBeginIndex is the first available entry in the
 *        buffer of vertices. This number is updated when
 *        a path is finished by adding the number of vertices
 *        in the path to the total length
 *  - vertexEndIndex is the number of entries in the buffer of
 *        path sets. This number is updated every time a new
 *        path type is begun.
 */
struct PTPathSet
{
    RTXDI_PTPathTraceInvocationType type;
    uint vertexBeginIndex;
    uint vertexEndIndex;
};

#endif // PT_PATH_SET_RECORD_H