#include "Common.hlsl"
#include "Random.hlsl"

[shader("closesthit")] 
void ComputeAo(inout HitInfo payload, Attributes attrib) 
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
      // float2 sample = theBuffers[mySampleBufferIndex].Load<float2>(i * sizeof(float2)) * 2 - 1;
      float2 randVec = float2(GetRand01(payload.myRngState), GetRand01(payload.myRngState)) * 2 - 1;
      // sample = reflect(sample, randVec);
      float2 sample = randVec;
    
      float3 dir = GetHemisphereDirection(sample, vertexData.myNormal);

      RayDesc rayDesc;
      rayDesc.Origin = pos;
      rayDesc.TMin = 0.01;
      rayDesc.Direction = dir;
      rayDesc.TMax = myAoDistance;

      TraceRay(theRtAccelerationStructures[myAsIndex],
            0,
            0xFF,
            myRayHitGroupOffset,
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

[shader("closesthit")] 
void ComputeIrradiance(inout HitInfo payload, Attributes attrib) 
{
  uint instanceId = InstanceID();
  InstanceData instanceData = LoadInstanceData(instanceId);
  MaterialData matData = LoadMaterialData(instanceData.myMaterialIndex);

  uint primitiveIndex = PrimitiveIndex();
  VertexData vertexData = LoadInterpolatedVertexData(instanceData.myVertexBufferIndex, instanceData.myIndexBufferIndex, primitiveIndex, attrib.bary);

  uint numHemisphereRays = 1;
  
  if (payload.myRecursionDepth < myMaxRecursionDepth) 
  {
      float3 pos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

      HitInfo hemispherePayload = (HitInfo) 0;
      hemispherePayload.myRngState = payload.myRngState;
      hemispherePayload.myRecursionDepth = payload.myRecursionDepth + 1;

      int numValidSamples = 0;
      // Sample Hemisphere
      for (uint i = 0; i < numHemisphereRays; ++i)
      {
        float2 randVec = float2(GetRand01(hemispherePayload.myRngState), GetRand01(hemispherePayload.myRngState)) * 2 - 1;

        float2 sample = randVec;
      
        float3 dir = GetHemisphereDirection(sample, vertexData.myNormal);

        RayDesc rayDesc;
        rayDesc.Origin = pos;
        rayDesc.TMin = 0.0001;
        rayDesc.Direction = dir;
        rayDesc.TMax = 99.0;

        HitInfo currRayPayload = hemispherePayload;
        currRayPayload.colorAndDistance = float4(0,0,0,0);

        TraceRay(theRtAccelerationStructures[myAsIndex],
              0,
              0xFF,
              myRayHitGroupOffset,
              0,
              0,
              rayDesc,
              currRayPayload);

        if (length(currRayPayload.colorAndDistance.xyz) > 0)
          numValidSamples++;

        hemispherePayload.colorAndDistance.xyz += currRayPayload.colorAndDistance.xyz;
      }

      float3 irradiance = numValidSamples > 0 ? hemispherePayload.colorAndDistance.xyz / numValidSamples : float3(0,0,0);  // / PI ?
      // Compute radiance into originating ray-direction
      float3 radiance = irradiance * max(0, dot(-WorldRayDirection(), vertexData.myNormal)) * matData.myColor.xyz + matData.myEmission;
      payload.colorAndDistance.xyz += radiance;
    }
}
