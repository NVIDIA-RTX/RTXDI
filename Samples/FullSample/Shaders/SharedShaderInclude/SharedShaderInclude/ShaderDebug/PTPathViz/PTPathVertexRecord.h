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

#ifndef PT_VERTEX_RECORD_H
#define PT_VERTEX_RECORD_H

struct PTPathVertexRecord
{
    float3 position;
    float3 normal;
    float3 neeLightPosition;
    float3 pad;
};

#endif // PT_VERTEX_RECORD_H