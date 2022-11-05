#include "fancy/resources/shaders/GlobalResources.h"

cbuffer CB0 : register(b0, Space_LocalCBuffer)
{
  bool myIsBGR;
  uint mySrcTextureIdx;
  uint myLinearClampSamplerIndex;
  uint myNumAccumulationFrames;

  float2 myPixelToUv;
};

struct VS_OUT
{
  float4 pos : SV_POSITION;
};
//---------------------------------------------------------------------------//
#if defined(PROGRAM_TYPE_VERTEX)

struct VS_IN
{
  float2 position : POSITION;
};

VS_OUT main(VS_IN v)
{
  VS_OUT vs_out = (VS_OUT)0;
  vs_out.pos = float4(v.position.x, v.position.y, 1, 1);
  return vs_out;
}

#endif // PROGRAM_TYPE_VERTEX
//---------------------------------------------------------------------------//

#if defined(PROGRAM_TYPE_FRAGMENT)  

float4 main(VS_OUT fs_in) : SV_TARGET
{
  SamplerState linearClampSampler = theSamplers[myLinearClampSamplerIndex];
  float2 uv = myPixelToUv * int2(fs_in.pos.xy);
	float3 hdrColor = theTextures2D[mySrcTextureIdx].SampleLevel(linearClampSampler, uv, 0).xyz;
  float3 sdrColor = hdrColor / (hdrColor + 1);
  return float4(sdrColor.rgb, 1.f);
}

#endif // PROGRAM_TYPE_FRAGMENT
//---------------------------------------------------------------------------//  
