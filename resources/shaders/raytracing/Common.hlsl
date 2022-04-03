#ifndef INC_RT_COMMON
#define INC_RT_COMMON

#include "fancy/resources/shaders/GlobalResources.h"
#include "fancy/resources/shaders/Encoding.h"

cbuffer Constants : register(b0, Space_LocalCBuffer)
{
  float3 myNearPlaneCorner;
  bool myIsBGR;

  float3 myXAxis;
  uint myOutTexIndex;

  float3 myYAxis;
  uint myAsIndex;

  float3 myCameraPos;
  uint myInstanceDataBufferIndex;

  uint myMaterialDataBufferIndex;
};

struct HitInfo
{
  float4 colorAndDistance;
};

struct Attributes
{
  float2 bary;
};

struct VertexData
{
  float3 myNormal;
  float2 myUv;
};

struct InstanceData
{
  uint myIndexBufferIndex;
  uint myVertexBufferIndex;
  uint myMaterialIndex;
};

InstanceData LoadInstanceData(uint anInstanceId)
{
  return theBuffers[myInstanceDataBufferIndex].Load<InstanceData>(anInstanceId * sizeof(InstanceData));
}

struct MaterialDataEncoded
{
  uint myColor;
};

struct MaterialData
{
  float4 myColor;
};

MaterialData LoadMaterialData(uint aMaterialIndex)
{
  MaterialDataEncoded enc = theBuffers[myMaterialDataBufferIndex].Load<MaterialDataEncoded>(aMaterialIndex * sizeof(MaterialDataEncoded));
  
  MaterialData data;
  data.myColor = Decode_Unorm_RGBA(enc.myColor);
  return data;
};

uint3 LoadTriangleIndices(uint anIndexBufferIndex, uint aTriangleIndex)
{
  return theBuffers[NonUniformResourceIndex(anIndexBufferIndex)].Load<uint3>(aTriangleIndex * sizeof(uint3));
}

VertexData LoadVertexData(uint aVertexBufferIndex, uint aVertexIndex)
{
  return theBuffers[NonUniformResourceIndex(aVertexBufferIndex)].Load<VertexData>(aVertexIndex * sizeof(VertexData));
}

VertexData LoadInterpolatedVertexData(uint aVertexBufferIndex, uint anIndexBufferIndex, uint aTriangleIndex, float2 aBarycentrics)
{
  uint3 indices = LoadTriangleIndices(anIndexBufferIndex, aTriangleIndex);

  VertexData vertexDatas[3];
  vertexDatas[0] = LoadVertexData(aVertexBufferIndex, indices.x);
  vertexDatas[1] = LoadVertexData(aVertexBufferIndex, indices.y);
  vertexDatas[2] = LoadVertexData(aVertexBufferIndex, indices.z);

  float baryZ = 1 - (aBarycentrics.x + aBarycentrics.y);
  VertexData returnData;
  returnData.myNormal = aBarycentrics.x * vertexDatas[0].myNormal + aBarycentrics.y * vertexDatas[1].myNormal + baryZ * vertexDatas[2].myNormal;
  returnData.myUv = aBarycentrics.x * vertexDatas[0].myUv + aBarycentrics.y * vertexDatas[1].myUv + baryZ * vertexDatas[2].myUv;

  return returnData;
}

#endif  // INC_RT_COMMON