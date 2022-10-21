#ifndef INC_COMPUTE_TRANSMITTANCE_LUT
#define INC_COMPUTE_TRANSMITTANCE_LUT

#include "../../../fancy/resources/shaders/GlobalResources.h"
#include "../../../fancy/resources/shaders/common_types.h"
#include "Common.hlsl"

cbuffer CONSTANT_BUFFER : register(b0, Space_LocalCBuffer)
{
	AtmosphereParameters myAtmosphereParameters;
	uint2 myTransmittanceTextureRes;
	uint myOutTexIdx;
};

[numthreads(8, 8, 1)]
void main(uint3 aDTid : SV_DispatchThreadID)
{
	float2 pixPos = aDTid.xy + 0.5;

	// Compute camera position from LUT coords
	float2 uv = pixPos / float2(myTransmittanceTextureRes.x, myTransmittanceTextureRes.y);
	float viewHeight;
	float viewZenithCosAngle;
	UvToLutTransmittanceParams(myAtmosphereParameters, viewHeight, viewZenithCosAngle, uv);

	//  A few extra needed constants
	float3 posPlanetSpace = float3(0.0f, viewHeight, 0.0f);
	float3 worldDir = float3(0.0f, viewZenithCosAngle, sqrt(1.0 - viewZenithCosAngle * viewZenithCosAngle));

	const bool ground = false;
	const float sampleCountIni = 40.0f;	// Can go a low as 10 sample but energy lost starts to be visible.
	const float depthBufferValue = -1.0;
	const bool variableSampleCount = false;
	const float2 minMaxSPP = float2(0, 0);  // only needed with variableSampleCount
	const bool mieRayPhase = false;
	const float2 clipPos = float2(0, 0);
	const float3 sunDir = float3(0, 0, 0);
	const float4x4 invViewProjMat = (float4x4) 0;
	const float3 sunIllumination = float3(0, 0, 0);

	float3 opticalDepth = IntegrateScatteredLuminance(
		clipPos, 
		posPlanetSpace,
		worldDir, 
		sunDir,
		myAtmosphereParameters,
		ground,
		sampleCountIni,
		depthBufferValue,
		variableSampleCount,
		mieRayPhase,
		invViewProjMat,
		minMaxSPP,
		sunIllumination,
		0, 
		0).OpticalDepth;

	float3 transmittance = exp(-opticalDepth);

	// Optical depth to transmittance
	theRwTextures2D[myOutTexIdx][aDTid.xy] = float4(transmittance, 1.0);	
}

#endif  // INC_COMPUTE_TRANSMITTANCE_LUT