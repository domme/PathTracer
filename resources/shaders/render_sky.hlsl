#include "fancy/resources/shaders/GlobalResources.h"
#include "resources/shaders/sky/Common_Functions.hlsl"

cbuffer CB0 : register(b0, Space_LocalCBuffer)
{
	float2 myInvResolution;
	uint myOutTexIdx;
	uint mySkyViewLutTextureIndex;

	float4x4 myInvViewProj;
	
	float3 myViewPos;
	float myAtmosphereBottomRadius;

	float3 mySunDirection;
	uint myLinearClampSamplerIndex;

	uint2 mySkyViewLutTextureRes;
};

[numthreads(8, 8, 1)]
void main(uint3 aDTid : SV_DispatchThreadID)
{
	float2 pixPos = aDTid.xy + 0.5;
	float3 clipSpace = float3((pixPos * myInvResolution) * float2(2.0, -2.0) - float2(1.0, -1.0), 1.0);
	float4 farPlaneWorldPos = mul(myInvViewProj, float4(clipSpace, 1.0));
	farPlaneWorldPos.xyz /= farPlaneWorldPos.w;

	float3 viewDir = normalize(farPlaneWorldPos.xyz - myViewPos);
	float3 viewPos = myViewPos;
	viewPos.y += myAtmosphereBottomRadius;

	float viewHeight = length(viewPos);

	float3 upVector = normalize(viewPos);
	float viewZenithCosAngle = dot(viewDir, upVector);

	float3 sideVector = normalize(cross(upVector, viewDir));		// assumes non parallel vectors
	float3 forwardVector = normalize(cross(sideVector, upVector));	// aligns toward the sun light but perpendicular to up vector
	float2 lightOnPlane = float2(dot(mySunDirection, forwardVector), dot(mySunDirection, sideVector));
	lightOnPlane = normalize(lightOnPlane);
	float lightViewCosAngle = lightOnPlane.x;

	bool IntersectGround = raySphereIntersectNearest(viewPos, viewDir, float3(0, 0, 0), myAtmosphereBottomRadius) >= 0.0f;

	float2 uv;
	SkyViewLutParamsToUv(myAtmosphereBottomRadius, mySkyViewLutTextureRes, IntersectGround, viewZenithCosAngle, lightViewCosAngle, viewHeight, uv);

	SamplerState linearClampSampler = theSamplers[myLinearClampSamplerIndex];
	float3 luminance = theTextures2D[mySkyViewLutTextureIndex].SampleLevel(linearClampSampler, uv, 0).xyz;

    theRwTextures2D[myOutTexIdx][aDTid.xy] = float4(luminance, 1.0);
}
