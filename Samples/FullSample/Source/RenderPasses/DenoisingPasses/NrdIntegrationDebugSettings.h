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

struct NrdIntegrationDebugSettings
{
    // Enables validation layer, which writes buffer
    // output to a 4x4 full screen image that can be
    // viewed in the debug buffer blit mode
    bool enableValidation = false;

    // False = show just NRD validation
    // True  = show NRD validation on top of final image
    bool overlayOnFinalImage = true;

    // Ratio of the screen to show with NRD disabled vs enabled
    // 0.0 = full screen NRD on
    // 0.x = lefthand side enabled, righthand side disabled
    // 1.0 = full screen NRD off
    float splitScreen = 0.0f;
};