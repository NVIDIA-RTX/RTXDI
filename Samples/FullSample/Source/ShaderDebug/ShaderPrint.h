//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <nvrhi/nvrhi.h>
#include <donut/core/math/math.h>

#include "SharedShaderInclude/ShaderDebug/ShaderDebugPrintShared.h"

namespace donut::engine
{
    class Scene;
    class CommonRenderPasses;
    class ShaderFactory;
    class IView;
}

class ShaderPrintReadbackPass
{
public:
    ShaderPrintReadbackPass(
        nvrhi::IDevice* device,
        std::shared_ptr<donut::engine::ShaderFactory> shaderFactory,
        uint32_t bufferSizeInBytes,
        uint32_t numFramesInFlight);

    // Run at the very beginning of the frame to set the constant buffer
    void InitializeFrame(nvrhi::ICommandList* commandList);

    // Run at the very end of the frame to read back the print buffer
    void ReadBackPrintData(nvrhi::ICommandList* commandList);

    nvrhi::BufferHandle GetCBDataBuffer() const;
    nvrhi::BufferHandle GetPrintBuffer() const;

    uint32_t GetBufferSizeInBytes() const;
    donut::math::uint2 GetSelectedPixelCoordinates() const;
    void SetSelectedPixelCoordinates(donut::math::uint2 pixelCoordinates);

    void Enable(bool enable);
    bool IsEnabled() const;

    const std::vector<std::string>& GetLines() const;

private:
    nvrhi::IDevice* m_device;
    std::shared_ptr<donut::engine::ShaderFactory> m_shaderFactory;

    uint32_t m_numFramesInFlight;
    uint32_t m_currentFrameIndex;

    ShaderPrintCBData m_cbData;

    nvrhi::BufferHandle m_cbDataBuffer;
    nvrhi::BufferHandle m_printBuffer;
    std::vector<nvrhi::BufferHandle> m_printReadbackBuffers;

    std::vector<std::string> m_lines;
};