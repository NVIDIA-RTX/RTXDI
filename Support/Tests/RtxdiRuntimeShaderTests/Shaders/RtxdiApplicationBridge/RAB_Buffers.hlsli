#ifndef RAB_BUFFER_HLSLI
#define RAB_BUFFER_HLSLI

RWBuffer<uint2> u_RisBuffer : register(u10);
RWStructuredBuffer<RTXDI_PackedDIReservoir> u_LightReservoirs : register(u0);
Buffer<float2> t_NeighborOffsets : register(t21);
RWStructuredBuffer<RTXDI_PackedGIReservoir> u_GIReservoirs : register(u6);
RWStructuredBuffer<RTXDI_PackedPTReservoir> u_PTReservoirs : register(u7);

#define RTXDI_RIS_BUFFER u_RisBuffer
#define RTXDI_LIGHT_RESERVOIR_BUFFER u_LightReservoirs
#define RTXDI_NEIGHBOR_OFFSETS_BUFFER t_NeighborOffsets
#define RTXDI_GI_RESERVOIR_BUFFER u_GIReservoirs
#define RTXDI_PT_RESERVOIR_BUFFER u_PTReservoirs

#define IES_SAMPLER s_EnvironmentSampler

// Translates the light index from the current frame to the previous frame (if currentToPrevious = true)
// or from the previous frame to the current frame (if currentToPrevious = false).
// Returns the new index, or a negative number if the light does not exist in the other frame.
int RAB_TranslateLightIndex(uint lightIndex, bool currentToPrevious)
{
    return 0;
}

uint RAB_GetDuplicationMapCount(int2 prevPixelPos)
{
    return 0;
}

#endif // RAB_BUFFER_HLSLI
