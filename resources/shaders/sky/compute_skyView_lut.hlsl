#ifndef INC_COMPUTE_SKYVIEW_LUT
#define INC_COMPUTE_SKYVIEW_LUT

#include "../../../fancy/resources/shaders/GlobalResources.h"
#include "../../../fancy/resources/shaders/common_types.h"
#include "Common.hlsl"

cbuffer CONSTANT_BUFFER : register(b0, Space_LocalCBuffer)
{
	AtmosphereParameters myAtmosphereParameters;

	float4x4 myInvViewProj;

	float3 mySunIlluminance;
	uint myLinearClampSamplerIdx;

	float3 mySunDirection;
	uint myTransmissionLutTexIdx;

	float3 myCameraPos;
	uint myOutTexIdx;

	float2 myRayMarchMinMaxSPP;
	uint2 mySkyViewTextureRes;
};

[numthreads(8, 8, 1)]
void main(uint3 aDTid : SV_DispatchThreadID)
{
	float2 pixPos = aDTid.xy;
	float2 uv = pixPos / float2(mySkyViewTextureRes.x, mySkyViewTextureRes.y);

	float3 clipPos = float3(uv * float2(2.0, -2.0) - float2(1.0, -1.0), 1.0);
	float4 pixelPosWorld = mul(myInvViewProj, float4(clipPos, 1.0));
	pixelPosWorld.xyz /= pixelPosWorld.w;

	float3 viewDir = normalize(pixelPosWorld.xyz - myCameraPos);
	float3 worldPos = myCameraPos + float3(0, myAtmosphereParameters.BottomRadius, 0);
	float viewHeight = length(worldPos);
	float3 upVector = worldPos / viewHeight;

	float viewZenithCosAngle;
	float lightViewCosAngle;
	UvToSkyViewLutParams(myAtmosphereParameters.BottomRadius, myAtmosphereParameters.TopRadius, viewZenithCosAngle, lightViewCosAngle, viewHeight, uv, float2(mySkyViewTextureRes.x, mySkyViewTextureRes.y));

	float sunZenithCosAngle = dot(upVector, mySunDirection);
	float3 sunDir = normalize(float3(sqrt(1.0 - sunZenithCosAngle * sunZenithCosAngle), sunZenithCosAngle, 0.0));

	float viewZenithSinAngle = sqrt(1 - viewZenithCosAngle * viewZenithCosAngle);
	float3 worldDir = float3(
		viewZenithSinAngle * lightViewCosAngle,
		viewZenithCosAngle,
		viewZenithSinAngle * sqrt(1.0 - lightViewCosAngle * lightViewCosAngle));

	worldPos = float3(0.0, viewHeight, 0.0);

	// Move to top atmospehre
	if (!MoveToTopAtmosphere(worldPos, worldDir, myAtmosphereParameters.TopRadius))
	{
		// Ray is not intersecting the atmosphere
		theRwTextures2D[myOutTexIdx][aDTid.xy] = float4(0.0, 0.0, 0.0, 1.0);
	}
	else
	{
		const bool ground = false;
		const float sampleCountIni = 30;
		const float depthBufferValue = -1.0;
		const bool variableSampleCount = true;
		const bool mieRayPhase = true;
		SingleScatteringResult ss = IntegrateScatteredLuminance(
			clipPos.xy,
			worldPos,
			worldDir,
			mySunDirection,
			myAtmosphereParameters,
			ground,
			sampleCountIni,
			depthBufferValue,
			variableSampleCount,
			mieRayPhase,
			myInvViewProj,
			myRayMarchMinMaxSPP,
			mySunIlluminance,
			myLinearClampSamplerIdx,
			myTransmissionLutTexIdx);

		float3 L = ss.L;

		theRwTextures2D[myOutTexIdx][aDTid.xy] = float4(L, 1.0);
	}
}

#endif  // INC_COMPUTE_SKYVIEW_LUT