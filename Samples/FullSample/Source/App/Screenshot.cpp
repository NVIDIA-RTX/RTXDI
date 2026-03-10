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

#include "Screenshot.h"

#include <filesystem>

#include "donut/core/log.h"
#include "nvrhi/nvrhi.h"

#include <stb_image.h>
#include <stb_image_write.h>

using namespace donut;
namespace fs = std::filesystem;

bool SaveScreenshot(nvrhi::IDevice* device, nvrhi::ITexture* texture, const char* writeFileName)
{
    nvrhi::TextureDesc desc = texture->getDesc();
    nvrhi::FramebufferHandle tempFramebuffer;

    nvrhi::CommandListHandle commandList = device->createCommandList();
    commandList->open();
    
    nvrhi::StagingTextureHandle stagingTexture = device->createStagingTexture(desc, nvrhi::CpuAccessMode::Read);
    commandList->copyTexture(stagingTexture, nvrhi::TextureSlice(), texture, nvrhi::TextureSlice());
    
    commandList->close();
    device->executeCommandList(commandList);
    device->waitForIdle();

    size_t rowPitch = 0;
    void* pData = device->mapStagingTexture(stagingTexture, nvrhi::TextureSlice(), nvrhi::CpuAccessMode::Read, &rowPitch);

    if (!pData)
    {
        log::error("Couldn't map the readback texture.");
        return false;
    }

    //uint32_t* textureInSysmem = new uint32_t[desc.width * desc.height];
    std::vector<uint32_t> textureInSysmem(desc.width * desc.height);

    if (!textureInSysmem.data())
    {
        log::error("Couldn't allocate the memory for a %dx%d texture on the CPU.", desc.width, desc.height);
        return false;
    }
    
    for (uint32_t row = 0; row < desc.height; row++)
    {
        memcpy(textureInSysmem.data() + row * desc.width, static_cast<char*>(pData) + row * rowPitch, desc.width * sizeof(uint32_t));
    }

    device->unmapStagingTexture(stagingTexture);
    
    
    bool success = true;
    if (writeFileName && *writeFileName)
    {
        fs::path parentFolder = fs::path(writeFileName).parent_path();
        if (!parentFolder.empty() && !fs::exists(parentFolder))
        {
            log::info("Creating folder '%s'", parentFolder.generic_string().c_str());
            fs::create_directories(parentFolder);
        }

        success = stbi_write_bmp(writeFileName, desc.width, desc.height, 4, pData) != 0;
        if (success)
            log::info("Saved the screenshot into '%s'", writeFileName);
        else
            log::error("Failed to save the screenshot into '%s'", writeFileName);
    }

    return success;
}
