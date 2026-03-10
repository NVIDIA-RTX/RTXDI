#ifndef RAB_RAY_PAYLOAD_HLSLI
#define RAB_RAY_PAYLOAD_HLSLI

struct RAB_RayPayload
{
    uint unused;
};

float RAB_RayPayloadGetCommittedHitT(RAB_RayPayload payload)
{
    return 0;
}

#endif // RAB_RAY_PAYLOAD_HLSLI
