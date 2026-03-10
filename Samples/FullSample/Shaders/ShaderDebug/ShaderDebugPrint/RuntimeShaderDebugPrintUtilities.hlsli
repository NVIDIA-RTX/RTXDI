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

#ifndef RUNTIME_SHADER_DEBUG_PRINT_UTILITIES_HLSLI
#define RUNTIME_SHADER_DEBUG_PRINT_UTILITIES_HLSLI

void PrintPathType(RTXDI_PTPathTraceInvocationType type)
{
    switch(type)
    {
    case RTXDI_PTPathTraceInvocationType_Initial:
        DebugPrint_("Initial");
        break;
    case RTXDI_PTPathTraceInvocationType_Temporal:
        DebugPrint_("Temporal");
        break;
    case RTXDI_PTPathTraceInvocationType_TemporalInverse:
        DebugPrint_("Temporal Inverse");
        break;
    case RTXDI_PTPathTraceInvocationType_Spatial:
        DebugPrint_("Spatial");
        break;
    case RTXDI_PTPathTraceInvocationType_SpatialInverse:
        DebugPrint_("Spatial Inverse");
        break;
    case RTXDI_PTPathTraceInvocationType_DebugTemporalRetrace:
        DebugPrint_("Temporal Retrace");
        break;
    case RTXDI_PTPathTraceInvocationType_DebugSpatialRetrace:
        DebugPrint_("Spatial Retrace");
        break;
    }
}

void PrintReservoir_(RTXDI_PTReservoir r)
{
    DebugPrint_("Reservoir:");
    DebugPrint_("- TWP: {0}", r.TranslatedWorldPosition);
    DebugPrint_("- WS: {0}", r.WeightSum);
    DebugPrint_("- WN: {0}", r.WorldNormal);
    DebugPrint_("- M: {0}", r.M);
    DebugPrint_("- R: {0}", r.Radiance);
    DebugPrint_("- Age: {0}", r.Age);
    DebugPrint_("- SBSS: {0}", r.ShouldBoostSpatialSamples);
    DebugPrint_("- RcWiPdf: {0}", r.RcWiPdf);
    DebugPrint_("- PJ: {0}", r.PartialJacobian);
    DebugPrint_("- RcVL: {0}", r.RcVertexLength);
    DebugPrint_("- PL: {0}", r.PathLength);
    DebugPrint_("- RS: {0}", r.RandomSeed);
    DebugPrint_("- RI: {0}", r.RandomIndex);
    DebugPrint_("- TF: {0}", r.TargetFunction);
}

#endif // RUNTIME_SHADER_DEBUG_PRINT_UTILITIES_HLSLI
