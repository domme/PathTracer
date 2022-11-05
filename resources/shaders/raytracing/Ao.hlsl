#include "Common.hlsl"
#include "fancy/resources/shaders/GlobalResources.h"
#include "Random.hlsl"
#include "SampleSky.hlsl"

struct HitInfoPrimary
{
    float3 myHitPos;
    float3 myHitNormal;
    bool myHasHit;
};

[shader("closesthit")] 
void ClosestHitPrimary(inout HitInfoPrimary payload, Attributes attrib)
{
    uint instanceId = InstanceID();
    InstanceData instanceData = LoadInstanceData(instanceId);
    MaterialData matData = LoadMaterialData(instanceData.myMaterialIndex);

    uint primitiveIndex = PrimitiveIndex();
    VertexData vertexData = LoadInterpolatedVertexData(instanceData.myVertexBufferIndex, instanceData.myIndexBufferIndex, primitiveIndex, attrib.bary);

    payload.myHasHit = true;
    payload.myHitNormal = vertexData.myNormal;
    payload.myHitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
}

struct HitInfoAo 
{
    bool myHasHit;
};

[shader("closesthit")] 
void ClosestHitAo(inout HitInfoAo payload, Attributes attrib) 
{
    payload.myHasHit = true;
}

[shader("raygeneration")] 
void RayGen() 
{
    uint2 uPixel = DispatchRaysIndex().xy;
    uint2 resolution = DispatchRaysDimensions().xy;
    RngStateType rngState = InitRNG(uPixel, resolution, myFrameRandomSeed);

    float2 pixel = uPixel;

    float2 jitter = float2(GetRand01(rngState), GetRand01(rngState));
    pixel += lerp(-0.5.xx, 0.5.xx, jitter);
    pixel = clamp(pixel, 0, resolution);

    float3 origin;
    float3 dir;
    GetPrimaryRay(pixel, resolution, origin, dir);

    RayDesc rayDesc;
    rayDesc.Origin = origin;
    rayDesc.TMin = 0.0;
    rayDesc.Direction = dir;
    rayDesc.TMax = 10000.0;  

    HitInfoPrimary primaryHitInfo;
    primaryHitInfo.myHasHit = false;

    float3 pixelLuminance = float3(0, 0, 0);

    TraceRay(theRtAccelerationStructures[myAsIndex], 0, 0xFF, 0, 0, 0, rayDesc, primaryHitInfo);

    if (primaryHitInfo.myHasHit)
    {
        const uint numAoRays = 16;
        uint numAoHits = 0;
        for (uint i = 0u; i < numAoRays; ++i) 
        {
            float2 rand11 = float2(GetRand01(rngState), GetRand01(rngState)) * 2 - 1;
            float3 dir = GetHemisphereDirection(rand11, primaryHitInfo.myHitNormal);

            rayDesc.Origin = primaryHitInfo.myHitPos;
            rayDesc.TMin = 0.01;
            rayDesc.Direction = dir;
            rayDesc.TMax = myAoDistance;

            HitInfoAo aoHitInfo;
            aoHitInfo.myHasHit = false;

            TraceRay(theRtAccelerationStructures[myAsIndex], 0, 0xFF, 1, 0, 0, rayDesc, aoHitInfo);
            if (aoHitInfo.myHasHit)
                ++numAoHits;
        }

        float ao = 1 - float(numAoHits) / float(numAoRays); 
        pixelLuminance = float3(ao, ao, ao);
    }
    else 
    {
        pixelLuminance = SampleSkyLuminance(origin, dir);
    }

    float4 accumLight = theRwTextures2D[myOutTexIndex][pixel] * myNumAccumulationFrames;
    accumLight += float4(pixelLuminance, 0.0);
    accumLight /= float(myNumAccumulationFrames + 1);
    theRwTextures2D[myOutTexIndex][pixel] = accumLight;
}