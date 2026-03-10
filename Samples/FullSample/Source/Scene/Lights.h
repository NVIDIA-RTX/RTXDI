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

#include <donut/engine/Scene.h>

constexpr int LightType_Environment = 1000;
constexpr int LightType_Cylinder = 1001;
constexpr int LightType_Disk = 1002;
constexpr int LightType_Rect = 1003;

class SpotLightWithProfile : public donut::engine::SpotLight
{
public:
    std::string profileName;
    int profileTextureIndex = -1;

    void Load(const Json::Value& node) override;
    void Store(Json::Value& node) const override;
    [[nodiscard]] std::shared_ptr<SceneGraphLeaf> Clone() override;
};

class EnvironmentLight : public donut::engine::Light
{
public:
    dm::float3 radianceScale = 1.f;
    int textureIndex = -1;
    float rotation = 0.f;
    dm::uint2 textureSize = 0u;

    [[nodiscard]] int GetLightType() const override;
    [[nodiscard]] std::shared_ptr<SceneGraphLeaf> Clone() override;
};

class CylinderLight : public donut::engine::Light
{
public:
    float length = 1.f;
    float radius = 1.f;
    float flux = 1.f;

    void Load(const Json::Value& node) override;
    void Store(Json::Value& node) const override;
    [[nodiscard]] int GetLightType() const override;
    [[nodiscard]] std::shared_ptr<SceneGraphLeaf> Clone() override;
};

class DiskLight : public donut::engine::Light
{
public:
    float radius = 1.f;
    float flux = 1.f;

    void Load(const Json::Value& node) override;
    void Store(Json::Value& node) const override;
    [[nodiscard]] int GetLightType() const override;
    [[nodiscard]] std::shared_ptr<SceneGraphLeaf> Clone() override;
};

class RectLight : public donut::engine::Light
{
public:
    float width = 1.f;
    float height = 1.f;
    float flux = 1.f;

    void Load(const Json::Value& node) override;
    void Store(Json::Value& node) const override;
    [[nodiscard]] int GetLightType() const override;
    [[nodiscard]] std::shared_ptr<SceneGraphLeaf> Clone() override;
};
