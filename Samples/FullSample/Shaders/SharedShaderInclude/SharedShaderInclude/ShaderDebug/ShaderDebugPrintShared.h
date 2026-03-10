//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#ifndef SHADER_DEBUG_PRINT_SHARED_INCLUDE_HLSLI
#define SHADER_DEBUG_PRINT_SHARED_INCLUDE_HLSLI

#define SHADER_DEBUG_PRINT_ENABLED 0

struct ShaderPrintCBData
{
    uint32_t PrintBufferIndex;
    uint32_t PrintBufferSliceSizeInBytes;
    
#ifdef __cplusplus
    donut::math::uint2 CursorXY;
#else
    uint2 CursorXY;
#endif

    uint32_t enable;
    uint32_t pad1;
    uint32_t pad2;
    uint32_t pad3;
};

static uint32_t MaxDebugPrintArgs = 16;

enum ArgCode
{
    DebugPrint_Bool = 0,
    DebugPrint_Uint,
    DebugPrint_Uint2,
    DebugPrint_Uint3,
    DebugPrint_Uint4,
    DebugPrint_Int,
    DebugPrint_Int2,
    DebugPrint_Int3,
    DebugPrint_Int4,
    DebugPrint_Float,
    DebugPrint_Float2,
    DebugPrint_Float3,
    DebugPrint_Float4,

    NumDebugPrintArgCodes,
};

struct DebugPrintHeader
{
    uint32_t NumBytes;
    uint32_t StringSize;
    uint32_t NumArgs;
};

#endif // SHADER_DEBUG_PRINT_SHARED_INCLUDE_HLSLI
