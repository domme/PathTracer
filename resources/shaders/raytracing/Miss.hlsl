#include "Common.hlsl"
#include "../sky/Common.hlsl"

[shader("miss")]
void Miss(inout HitInfo payload : SV_RayPayload)
{
	float3 viewDir = WorldRayDirection();
	float3 worldPos = WorldRayOrigin() + float3(0, mySkyConsts.myAtmosphere.BottomRadius, 0);
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

    payload.colorAndDistance = float4(skyLuminance, -1.f);
}