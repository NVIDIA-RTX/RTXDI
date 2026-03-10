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

#define RTXDI_NVAPI_SHADER_EXT_SLOT 999
#define RTXDI_NVAPI_SHADER_EXT_SPACE_D3D12 0
#define RTXDI_NVAPI_SHADER_EXT_SPACE_VK 999
#define CONCAT(a,b) a##b
#define CONCAT_UAV(x) CONCAT(u,x)
#define CONCAT_SPACE(x) CONCAT(space,x)
#define NV_SHADER_EXTN_SLOT CONCAT_UAV(RTXDI_NVAPI_SHADER_EXT_SLOT)
#define NV_SHADER_EXTN_REGISTER_SPACE_D3D12 CONCAT_SPACE(RTXDI_NVAPI_SHADER_EXT_SPACE_D3D12)
#define NV_SHADER_EXTN_REGISTER_SPACE_VK CONCAT_SPACE(RTXDI_NVAPI_SHADER_EXT_SPACE_VK)