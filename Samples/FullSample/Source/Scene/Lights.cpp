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

#include "Lights.h"

#include <donut/core/json.h>

using namespace donut;

void SpotLightWithProfile::Load(const Json::Value& node)
{
    engine::SpotLight::Load(node);

    profileName = json::Read<std::string>(node["profile"], "");
}

void SpotLightWithProfile::Store(Json::Value& node) const
{
    engine::SpotLight::Store(node);

    node["profile"] = profileName;
}

std::shared_ptr<engine::SceneGraphLeaf> SpotLightWithProfile::Clone()
{
    auto copy = std::make_shared<SpotLightWithProfile>();
    copy->color = color;
    copy->intensity = intensity;
    copy->radius = radius;
    copy->range = range;
    copy->innerAngle = innerAngle;
    copy->outerAngle = outerAngle;
    copy->profileName = profileName;
    copy->profileTextureIndex = profileTextureIndex;
    return std::static_pointer_cast<SceneGraphLeaf>(copy);
}

int EnvironmentLight::GetLightType() const
{
    return LightType_Environment;
}

std::shared_ptr<engine::SceneGraphLeaf> EnvironmentLight::Clone()
{
    auto copy = std::make_shared<EnvironmentLight>();
    copy->color = color;
    copy->radianceScale = radianceScale;
    copy->textureIndex = textureIndex;
    copy->rotation = rotation;
    copy->textureSize = textureSize;
    return std::static_pointer_cast<SceneGraphLeaf>(copy);
}

void CylinderLight::Load(const Json::Value& node)
{
    node["color"] >> color;
    node["flux"] >> flux;
    node["radius"] >> radius;
    node["length"] >> length;
}

void CylinderLight::Store(Json::Value& node) const
{
    node["color"] << color;
    node["flux"] << flux;
    node["radius"] << radius;
    node["length"] << length;
}

int CylinderLight::GetLightType() const
{
    return LightType_Cylinder;
}

std::shared_ptr<engine::SceneGraphLeaf> CylinderLight::Clone()
{
    auto copy = std::make_shared<CylinderLight>();
    copy->color = color;
    copy->length = length;
    copy->radius = radius;
    copy->flux = flux;
    return std::static_pointer_cast<SceneGraphLeaf>(copy);
}

void DiskLight::Load(const Json::Value& node)
{
    node["color"] >> color;
    node["flux"] >> flux;
    node["radius"] >> radius;
}

void DiskLight::Store(Json::Value& node) const
{
    node["name"] << GetName();
    node["center"] << GetPosition();
    node["normal"] << GetDirection();
    node["color"] << color;
    node["flux"] << flux;
    node["radius"] << radius;
}

int DiskLight::GetLightType() const
{
    return LightType_Disk;
}

std::shared_ptr<engine::SceneGraphLeaf> DiskLight::Clone()
{
    auto copy = std::make_shared<DiskLight>();
    copy->color = color;
    copy->radius = radius;
    copy->flux = flux;
    return std::static_pointer_cast<SceneGraphLeaf>(copy);
}

void RectLight::Load(const Json::Value& node)
{
    node["color"] >> color;
    node["flux"] >> flux;
    node["width"] >> width;
    node["height"] >> height;
}

void RectLight::Store(Json::Value& node) const
{
    node["color"] << color;
    node["flux"] << flux;
    node["width"] << width;
    node["height"] << height;
}

int RectLight::GetLightType() const
{
    return LightType_Rect;
}

std::shared_ptr<engine::SceneGraphLeaf> RectLight::Clone()
{
    auto copy = std::make_shared<RectLight>();
    copy->color = color;
    copy->width = width;
    copy->height = height;
    copy->flux = flux;
    return std::static_pointer_cast<SceneGraphLeaf>(copy);
}