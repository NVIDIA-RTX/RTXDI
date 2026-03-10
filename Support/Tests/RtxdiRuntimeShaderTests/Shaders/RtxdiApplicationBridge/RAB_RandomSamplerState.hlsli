#ifndef RTXDI_RAB_RANDOM_SAMPLER_STATE_HLSLI
#define RTXDI_RAB_RANDOM_SAMPLER_STATE_HLSLI

struct RTXDI_RandomSamplerstate
{
    uint unused;
};

RTXDI_RandomSamplerstate RAB_InitRandomSampler(uint2 index, uint pass)
{
    RTXDI_RandomSamplerstate rng;
    return rng;
}

float RAB_GetNextRandom(inout RTXDI_RandomSamplerstate rng)
{
    return 0.0;
}

#endif // RTXDI_RAB_RANDOM_SAMPLER_STATE_HLSLI
