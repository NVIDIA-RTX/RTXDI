#ifndef RAB_PATH_TRACER_USER_DATA_HLSLI
#define RAB_PATH_TRACER_USER_DATA_HLSLI

struct RAB_PathTracerUserData
{
};

RAB_PathTracerUserData RAB_EmptyPathTracerUserData()
{
    RAB_PathTracerUserData ptud = (RAB_PathTracerUserData)0;
    return ptud;
}

void RAB_PathTracerUserDataSetPathType(inout RAB_PathTracerUserData ptud, RTXDI_PTPathTraceInvocationType type)
{
}

#endif // RAB_PATH_TRACER_USER_DATA_HLSLI
