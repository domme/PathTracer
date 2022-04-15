#ifndef INC_RT_COMMON
#define INC_RT_COMMON

#include "fancy/resources/shaders/GlobalResources.h"
#include "fancy/resources/shaders/Encoding.h"

static const float PI = 3.14159265f;

cbuffer Constants : register(b0, Space_LocalCBuffer)
{
  float3 myNearPlaneCorner;
  float myAoDistance;

  float3 myXAxis;
  uint myOutTexIndex;

  float3 myYAxis;
  uint myAsIndex;

  float3 myCameraPos;
  uint myInstanceDataBufferIndex;

  uint myMaterialDataBufferIndex;
  uint mySampleBufferIndex;
  uint myFrameRandomSeed;
  uint myNumAccumulationFrames;
};

struct HitInfo
{
  float4 colorAndDistance;
  uint4 myRngState;
  uint myRecursionDepth;
  uint myNumAoHits;
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
  float3 myEmission;
  uint myColor;
};

struct MaterialData
{
  float3 myEmission;
  float4 myColor;
};

MaterialData LoadMaterialData(uint aMaterialIndex)
{
  MaterialDataEncoded enc = theBuffers[myMaterialDataBufferIndex].Load<MaterialDataEncoded>(aMaterialIndex * sizeof(MaterialDataEncoded));
  
  MaterialData data;
  data.myEmission = enc.myEmission;
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

float3 GetRandomDirectionInSphere(float2 aRand01)
{
  float phi = aRand01.x * 2 * PI;
  float theta = aRand01.y * PI;
  return float3(sin(phi) * cos(theta), sin(phi) * sin(theta), cos(phi));
}

float3x3 GetCoordinateFrame(float3 aNormal)
{
  float3 side = abs(aNormal.x) < 0.999 ? float3(1, 0, 0) : float3(0, 0, 1);
  float3 z = normalize(cross(-aNormal, -side));
  float3 x = cross(z, aNormal);
  return float3x3(x, aNormal, z);
}

float3 GetHemisphereDirection(float2 aRand11, float3 aNormal)
{
  float2 rand = aRand11;
  float3 dir = float3(rand.x, 0.0, rand.y);
  dir.y = sqrt(1.0 - dot(dir.xz, dir.xz));

  float3x3 coordinateFrame = GetCoordinateFrame(aNormal);
  return normalize(mul(dir, coordinateFrame));
}

#endif  // INC_RT_COMMON