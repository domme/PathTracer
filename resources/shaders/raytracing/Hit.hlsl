#include "Common.hlsl"

[shader("closesthit")] 
void ClosestHit(inout HitInfo payload, Attributes attrib) 
{
  uint instanceId = InstanceID();
  InstanceData instanceData = LoadInstanceData(instanceId);
  MaterialData matData = LoadMaterialData(instanceData.myMaterialIndex);

  uint primitiveIndex = PrimitiveIndex();
  VertexData vertexData = LoadInterpolatedVertexData(instanceData.myVertexBufferIndex, instanceData.myIndexBufferIndex, primitiveIndex, attrib.bary);

  uint numAoRays = 16;

  if (payload.myRecursionDepth == 0)  // Primary ray
  {
    float3 pos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

    payload.myRecursionDepth = 1;
    payload.myNumAoHits = 0;

    // Sample Hemisphere to compute AO
    for (uint i = 0; i < numAoRays; ++i)
    {
      float2 sample = theBuffers[mySampleBufferIndex].Load<float2>(i * sizeof(float2));
      float3 dir = GetHemisphereDirection(sample, vertexData.myNormal);

      RayDesc rayDesc;
      rayDesc.Origin = pos;
      rayDesc.TMin = 0.01;
      rayDesc.Direction = dir;
      rayDesc.TMax = 30.0;

      TraceRay(theRtAccelerationStructures[myAsIndex],
            0,
            0xFF,
            0,
            0,
            0,
            rayDesc,
            payload);
    }

    float ao = 1 - float(payload.myNumAoHits) / float(numAoRays); 
    payload.colorAndDistance = float4(ao, ao, ao, RayTCurrent());  

  }
  else
  {
    payload.myNumAoHits += 1;
  }
}
