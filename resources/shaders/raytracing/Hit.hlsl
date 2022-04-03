#include "Common.hlsl"

[shader("closesthit")] 
void ClosestHit(inout HitInfo payload, Attributes attrib) 
{
  uint instanceId = InstanceID();
  InstanceData instanceData = LoadInstanceData(instanceId);
  MaterialData matData = LoadMaterialData(instanceData.myMaterialIndex);

  uint primitiveIndex = PrimitiveIndex();
  VertexData vertexData = LoadInterpolatedVertexData(instanceData.myVertexBufferIndex, instanceData.myIndexBufferIndex, primitiveIndex, attrib.bary);

  payload.colorAndDistance = float4(matData.myColor.xyz, RayTCurrent());
}



