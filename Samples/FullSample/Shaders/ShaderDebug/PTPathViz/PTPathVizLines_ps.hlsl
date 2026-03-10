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

#ifndef PT_PATH_VIZ_LINES_PS_HLSL
#define PT_PATH_VIZ_LINES_PS_HLSL

#include "SharedShaderInclude/ShaderDebug/PTPathViz/PTPathVizConstants.h"

ConstantBuffer<PTPathVizLineConstants> g_Const : register(b0);

void ps_main(
	in uint segmentID : SV_PrimitiveID,
    in float3 color : COLOR0,
	out float4 o_color : SV_TARGET0)
{
	o_color = float4(color, 1.0);
}


#endif // PT_PATH_VIZ_LINES_PS_HLSL