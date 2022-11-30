#ifndef INC_RT_COMMON
#define INC_RT_COMMON

#include "fancy/resources/shaders/GlobalResources.h"
#include "fancy/resources/shaders/Encoding.h"
#include "fancy/resources/shaders/common_types.h"
#include "../sky/Common.hlsl"
#include "random.hlsl"

struct SkyConstants
{
  AtmosphereParameters myAtmosphere;

	float3 mySunDirection;
	uint myTransmissionLutTexIdx;

  float3 mySunIlluminance;
  float _unused;

  float2 myRayMarchMinMaxSPP;
  float2 _unused2;
};

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

  uint myLinearClampSamplerIndex;
  uint myMaxRecursionDepth;
  uint myLightInstanceId;
  uint myNumHaltonSamples;

  float3 myLightEmission;
  uint mySampleSky;

  float3 mySkyFallbackEmission;
  float myPhongSpecularPower;

  SkyConstants mySkyConsts;
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

float3 GetUniformRandomDirectionInSphere(float2 aRand01)
{
	float phi = 2.0f * PI * aRand01.x;
	float theta = 2.0f * acos(sqrt(1.0f - aRand01.y));
	float3 dir = float3(sin(theta)*cos(phi), sin(theta)*sin(phi), cos(theta));
	return dir;
}

float3 GetCosineWeightedHemisphereDirection(float2 aRand01, float3 aNormal, float3 aPoint) 
{
  // https://twitter.com/Atrix256/status/1239634566559576065?s=20&t=9dP5EskBwlVQM67QnrCKkg
  return normalize(aNormal + GetUniformRandomDirectionInSphere(aRand01));
}

float3 GetRandomDirectionInSphere(float2 aRand01)
{
  float phi = aRand01.x * 2 * PI;
  float theta = aRand01.y * PI;
  return float3(sin(phi) * cos(theta), sin(phi) * sin(theta), cos(phi));
}

void GetCoordinateFrame(float3 aNormal, out float3 tangentOut, out float3 bitangentOut)
{
  float3 side = abs(aNormal.x) < 0.999 ? float3(1, 0, 0) : float3(0, 0, 1);
  float3 z = normalize(cross(-aNormal, -side));
  float3 x = cross(z, aNormal);
  tangentOut = x;
  bitangentOut = z;
}

float3 GetHemisphereDirection(float2 aRand11, float3 aNormal)
{
  float3 tangent;
  float3 bitangent;
  GetCoordinateFrame(aNormal, tangent, bitangent);
  
  return normalize( aNormal + tangent * aRand11.x + bitangent * aRand11.y );
}

float3 TransformToNormalFrame(float3 aNormal, float3 aDir) 
{
  float3 tangent;
  float3 bitangent;
  GetCoordinateFrame(aNormal, tangent, bitangent);

  float3x3 tbn = float3x3(tangent, aNormal, bitangent);
  return mul(tbn, aDir);
}

float2 GetHaltonSample( uint index ) {
  uint i = index % myNumHaltonSamples;
  return theBuffers[mySampleBufferIndex].Load<float2>( i * sizeof(float2));
}

void GetPrimaryRay(float2 pixel, uint2 resolution, out float3 origin, out float3 dir)
{
  float2 vpLerp = float2(pixel) / resolution;
  vpLerp.y = 1.0 - vpLerp.y;
  origin = myNearPlaneCorner + myXAxis * vpLerp.x + myYAxis * vpLerp.y;
  dir = normalize(origin - myCameraPos);
}

float GetLuminance(float3 radiance) 
{
  return dot(radiance, float3(0.2126f, 0.7152f, 0.0722f));
}

float GetFresnelSchlick(float3 aNormal, float3 aView) 
{
  const float f0 = 0.04f; // Assuming dielectrics for now
  float cosTheta = max( 0, dot(aNormal, aView) );
  return saturate( f0 + (1.0f - f0) * pow(1.0f - cosTheta, 5.0f) );
}

#endif  // INC_RT_COMMON