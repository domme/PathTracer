#include "Common.hlsl"
#include "fancy/resources/shaders/GlobalResources.h"

cbuffer Constants : register(b0, Space_LocalCBuffer)
{
  float3 myNearPlaneCorner;
  bool myIsBGR;

  float3 myXAxis;
  uint myOutTexIndex;

  float3 myYAxis;
  uint myAsIndex;

  float3 myCameraPos;
};

[shader("raygeneration")] 
void RayGen() 
{
  uint2 pixel = DispatchRaysIndex().xy;
  float2 vpLerp = float2(pixel) / DispatchRaysDimensions().xy;
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

  TraceRay( theRtAccelerationStructures[myAsIndex],
            0,
            0xFF,
            0,
            0,
            0,
            rayDesc,
            payload);

  theRwTextures2D[myOutTexIndex][pixel] = float4(myIsBGR ? payload.colorAndDistance.bgr : payload.colorAndDistance.rgb, 1.f);
}
