#include "Common.hlsl"
#include "fancy/resources/shaders/GlobalResources.h"
#include "Random.hlsl"
#include "SampleSky.hlsl"

struct HitInfo
{
    float3 myHitPos;
    float3 myHitNormal;
    float3 myColor;
    float3 myEmission;
    bool myHasHit;
};

[shader("closesthit")] 
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
    uint instanceId = InstanceID();
    InstanceData instanceData = LoadInstanceData(instanceId);
    MaterialData matData = LoadMaterialData(instanceData.myMaterialIndex);

    uint primitiveIndex = PrimitiveIndex();
    VertexData vertexData = LoadInterpolatedVertexData(instanceData.myVertexBufferIndex, instanceData.myIndexBufferIndex, primitiveIndex, attrib.bary);

    payload.myHasHit = true;
    payload.myHitNormal = vertexData.myNormal;
    payload.myHitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    payload.myColor = matData.myColor.xyz;
    payload.myEmission = matData.myEmission;

    if (dot(payload.myHitNormal, -WorldRayDirection()) < 0)
        payload.myHitNormal = -payload.myHitNormal;

    // Hacky: Replace a predefined instance with a light
    if (instanceId == myLightInstanceId)
        payload.myEmission = myLightEmission;
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

    HitInfo hitInfo;
    
    float3 luminance = float3(0, 0, 0);
    float3 transmission = float3(1, 1, 1);
    
    for ( uint bounceIdx = 0u; bounceIdx <= myMaxRecursionDepth; ++bounceIdx ) {
            
        hitInfo.myHasHit = false;
        TraceRay(theRtAccelerationStructures[myAsIndex], 0, 0xFF, 0, 0, 0, rayDesc, hitInfo);

        if (!hitInfo.myHasHit) 
        {
            luminance += transmission * SampleSkyLuminance(rayDesc.Origin, rayDesc.Direction);
            break;
        }

        if (hitInfo.myHasHit)
        {
            // TODO: Evaluate local lighting

            float3 brdf = GetLambertianBRDF( hitInfo.myColor, hitInfo.myHitNormal, -rayDesc.Direction );
            float pdf = GetLambertianPDF( hitInfo.myHitNormal, -rayDesc.Direction );

            transmission *= brdf / ( pdf > 0.0001f ? pdf : 1.0f);
            luminance += transmission * hitInfo.myEmission; 
        }

        // Check if ray should be terminated (russian roulette)
        const uint minBounces = 3;
        if (bounceIdx > minBounces) 
        {
            float terminationProbability = min(0.95, GetRand01(rngState));
            if (GetLuminance(transmission) < terminationProbability) {
                break;
            } else {
                transmission /= terminationProbability;
            }

        }
        
        float2 randVec = float2(GetRand01(rngState), GetRand01(rngState));
        rayDesc.Origin = hitInfo.myHitPos;
        rayDesc.Direction = GetCosineWeightedHemisphereDirection(randVec, hitInfo.myHitNormal, hitInfo.myHitPos);
        rayDesc.TMin = 0.001;
    }

    if (isnan(luminance.x) || isnan(luminance.y) || isnan(luminance.z))
        luminance = float3(0, 0, 0);

    float4 accumLight = theRwTextures2D[myOutTexIndex][pixel] * myNumAccumulationFrames;
    accumLight += float4(luminance, 0.0);
    accumLight /= float(myNumAccumulationFrames + 1);
    theRwTextures2D[myOutTexIndex][pixel] = accumLight;
}