#ifndef INC_RT_SAMPLESKY
#define INC_RT_SAMPLESKY

#include "Common.hlsl"
#include "../sky/Common.hlsl"

float3 SampleSkyLuminance(float3 viewPos, float3 viewDir) 
{
    if (!mySampleSky)
        return mySkyFallbackEmission;

	float3 worldPos = viewPos + float3(0, mySkyConsts.myAtmosphere.BottomRadius, 0);
	float viewHeight = length(worldPos);
	float3 upVector = worldPos / viewHeight;

    float3 skyLuminance = float3( 0, 0, 0 );

	// Move to top atmospehre
	if (MoveToTopAtmosphere(worldPos, viewDir, mySkyConsts.myAtmosphere.TopRadius))
	{
		const bool ground = false;
		const float sampleCountIni = 30;
		const float depthBufferValue = -1.0;
		const bool variableSampleCount = true;
		const bool mieRayPhase = true;
		SingleScatteringResult ss = IntegrateScatteredLuminance (
			float2( 0, 0 ),
			worldPos,
			viewDir,
			mySkyConsts.mySunDirection,
			mySkyConsts.myAtmosphere,
			ground,
			sampleCountIni,
			depthBufferValue,
			variableSampleCount,
			mieRayPhase,
			(float4x4) 0,
			mySkyConsts.myRayMarchMinMaxSPP,
			mySkyConsts.mySunIlluminance,
			myLinearClampSamplerIndex,
			mySkyConsts.myTransmissionLutTexIdx);

		skyLuminance = ss.L;
    }

    return skyLuminance;
}


#endif