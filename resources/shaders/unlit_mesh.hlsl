#include "fancy/resources/shaders/GlobalResources.h"

cbuffer CB0 : register(b0, Space_LocalCBuffer)
{
  float4x4 myWVP; 
  float4 myColor;
};

struct VS_OUT
{
  float4 pos : SV_POSITION;
};
//---------------------------------------------------------------------------//
  #if defined(PROGRAM_TYPE_VERTEX)

    struct VS_IN
    {
      float3 position : POSITION;
      float3 normal : NORMAL;
      float4 color : COLOR0;
    };
    
    VS_OUT main(VS_IN v)
    {
      VS_OUT vs_out = (VS_OUT)0;
      vs_out.pos = mul(myWVP, float4(v.position, 1.0f));
      return vs_out;
    }

  #endif // PROGRAM_TYPE_VERTEX
//---------------------------------------------------------------------------//
  #if defined(PROGRAM_TYPE_FRAGMENT)  

    float4 main(VS_OUT fs_in) : SV_TARGET
    {
      return myColor;
    }

  #endif // PROGRAM_TYPE_FRAGMENT
//---------------------------------------------------------------------------//  
