#include "Common.hlsl"
#include "fancy/resources/shaders/GlobalResources.h"
#include "Random.hlsl"

[shader("raygeneration")] 
void RayGen() 
{
  uint2 uPixel = DispatchRaysIndex().xy;
  uint2 resolution = DispatchRaysDimensions().xy;
  uint4 rngState = InitRNG(uPixel, resolution, myFrameRandomSeed);

  float2 pixel = uPixel;

  float2 jitter = float2(GetRand01(rngState), GetRand01(rngState));
  pixel += lerp(-0.5.xx, 0.5.xx, jitter);
  pixel = clamp(pixel, 0, resolution);

  float2 vpLerp = float2(pixel) / resolution;
  vpLerp.y = 1.0 - vpLerp.y;
  float3 vpPos = myNearPlaneCorner + myXAxis * vpLerp.x + myYAxis * vpLerp.y;
  float3 rayDir = normalize(vpPos - myCameraPos);

  RayDesc rayDesc;
  rayDesc.Origin = vpPos;
  rayDesc.TMin = 0.0;
  rayDesc.Direction = rayDir;
  rayDesc.TMax = 10000.0;  

  HitInfo payload;
  payload.colorAndDistance = float4(0, 0, 0, 0);
  payload.myRecursionDepth = 0;
  payload.myNumAoHits = 0;
  payload.myRngState = rngState;

  TraceRay( theRtAccelerationStructures[myAsIndex],
            0,
            0xFF,
            0,
            0,
            0,
            rayDesc,
            payload);

  float4 accumLight = theRwTextures2D[myOutTexIndex][pixel] * myNumAccumulationFrames;
  accumLight += float4(payload.colorAndDistance.xyz, 0.0);
  accumLight /= float(myNumAccumulationFrames + 1);

  theRwTextures2D[myOutTexIndex][pixel] = accumLight;
}
