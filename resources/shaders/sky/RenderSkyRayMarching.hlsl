#include "fancy/resources/shaders/GlobalResources.h"
#include "fancy/resources/shaders/common_types.h"
#include "SkyAtmosphereCommon.hlsl"
#include "RenderSkyCommon.hlsl"
#include "Common.hlsl"

struct SingleScatteringResult
{
	float3 L;						// Scattered light (luminance)
	float3 OpticalDepth;			// Optical depth (1/m)
	float3 Transmittance;			// Transmittance in [0,1] (unitless)
	float3 MultiScatAs1;

	float3 NewMultiScatStep0Out;
	float3 NewMultiScatStep1Out;
};

SingleScatteringResult IntegrateScatteredLuminance(
	in float2 pixPos, in float3 WorldPos, in float3 WorldDir, in float3 SunDir, in AtmosphereParameters Atmosphere,
	in bool ground, in float SampleCountIni, in float DepthBufferValue, in bool VariableSampleCount,
	in bool MieRayPhase, in float tMaxMax = 9000000.0f)
{
	SingleScatteringResult result = (SingleScatteringResult)0;

	// Compute next intersection with atmosphere or ground 
	float3 earthO = float3(0.0f, 0.0f, 0.0f);
	float tBottom = raySphereIntersectNearest(WorldPos, WorldDir, earthO, Atmosphere.BottomRadius);
	float tTop = raySphereIntersectNearest(WorldPos, WorldDir, earthO, Atmosphere.TopRadius);
	float tMax = 0.0f;
	if (tBottom < 0.0f)
	{
		if (tTop < 0.0f)
		{
			tMax = 0.0f; // No intersection with earth nor atmosphere: stop right away  
			return result;
		}
		else
		{
			tMax = tTop;
		}
	}
	else
	{
		if (tTop > 0.0f)
		{
			tMax = min(tTop, tBottom);
		}
	}

	if (DepthBufferValue >= 0.0f)
	{
		float3 ClipSpace = float3((pixPos / float2(gResolution))*float2(2.0, -2.0) - float2(1.0, -1.0), 1.0);
		ClipSpace.z = DepthBufferValue;
		if (ClipSpace.z < 1.0f)
		{
			float4 DepthBufferWorldPos = mul(gSkyInvViewProjMat, float4(ClipSpace, 1.0));
			DepthBufferWorldPos /= DepthBufferWorldPos.w;

			float tDepth = length(DepthBufferWorldPos.xyz - (WorldPos + float3(0.0, -Atmosphere.BottomRadius, 0.0))); // apply earth offset to go back to origin as top of earth mode. 
			if (tDepth < tMax)
			{
				tMax = tDepth;
			}
		}
	}
	tMax = min(tMax, tMaxMax);

	// Sample count 
	float SampleCount = SampleCountIni;
	float SampleCountFloor = SampleCountIni;
	float tMaxFloor = tMax;
	if (VariableSampleCount)
	{
		SampleCount = lerp(RayMarchMinMaxSPP.x, RayMarchMinMaxSPP.y, saturate(tMax*0.01));
		SampleCountFloor = floor(SampleCount);
		tMaxFloor = tMax * SampleCountFloor / SampleCount;	// rescale tMax to map to the last entire step segment.
	}
	float dt = tMax / SampleCount;

	// Phase functions
	const float uniformPhase = 1.0 / (4.0 * PI);
	const float3 wi = SunDir;
	const float3 wo = WorldDir;
	float cosTheta = dot(wi, wo);
	float MiePhaseValue = hgPhase(Atmosphere.MiePhaseG, -cosTheta);	// negate cosTheta because due to WorldDir being a "in" direction. 
	float RayleighPhaseValue = RayleighPhase(cosTheta);

#ifdef ILLUMINANCE_IS_ONE
	// When building the scattering factor, we assume light illuminance is 1 to compute a transfert function relative to identity illuminance of 1.
	// This make the scattering factor independent of the light. It is now only linked to the atmosphere properties.
	float3 globalL = 1.0f;
#else
	float3 globalL = gSunIlluminance;
#endif

	// Ray march the atmosphere to integrate optical depth
	float3 L = 0.0f;
	float3 throughput = 1.0;
	float3 OpticalDepth = 0.0;
	float t = 0.0f;
	const float SampleSegmentT = 0.3f;
	for (float s = 0.0f; s < SampleCount; s += 1.0f)
	{
		if (VariableSampleCount)
		{
			// More expenssive but artefact free
			float t0 = (s) / SampleCountFloor;
			float t1 = (s + 1.0f) / SampleCountFloor;
			// Non linear distribution of sample within the range.
			t0 = t0 * t0;
			t1 = t1 * t1;
			// Make t0 and t1 world space distances.
			t0 = tMaxFloor * t0;
			if (t1 > 1.0)
			{
				t1 = tMax;
				//	t1 = tMaxFloor;	// this reveal depth slices
			}
			else
			{
				t1 = tMaxFloor * t1;
			}
			t = t0 + (t1 - t0)*SampleSegmentT;
			dt = t1 - t0;
		}
		else
		{
			//t = tMax * (s + SampleSegmentT) / SampleCount;
			// Exact difference, important for accuracy of multiple scattering
			float NewT = tMax * (s + SampleSegmentT) / SampleCount;
			dt = NewT - t;
			t = NewT;
		}
		float3 P = WorldPos + t * WorldDir;

		MediumSampleRGB medium = sampleMediumRGB(P, Atmosphere);
		const float3 SampleOpticalDepth = medium.extinction * dt;
		const float3 SampleTransmittance = exp(-SampleOpticalDepth);
		OpticalDepth += SampleOpticalDepth;

#ifndef OPTICAL_DEPTH_ONLY

		float pHeight = length(P);
		const float3 UpVector = P / pHeight;
		float SunZenithCosAngle = dot(SunDir, UpVector);
		float2 uv;
		LutTransmittanceParamsToUv(Atmosphere, pHeight, SunZenithCosAngle, uv);
		SamplerState linearClampSampler = theSamplers[myLinearClampSamplerIndex];
		float3 TransmittanceToSun = theTextures2D[myTransmittanceLutTextureIndex].SampleLevel(linearClampSampler, uv, 0).rgb;

		float3 PhaseTimesScattering;
		if (MieRayPhase)
		{
			PhaseTimesScattering = medium.scatteringMie * MiePhaseValue + medium.scatteringRay * RayleighPhaseValue;
		}
		else
		{
			PhaseTimesScattering = medium.scattering * uniformPhase;
		}

		// Earth shadow 
		float tEarth = raySphereIntersectNearest(P, SunDir, earthO + PLANET_RADIUS_OFFSET * UpVector, Atmosphere.BottomRadius);
		float earthShadow = tEarth >= 0.0f ? 0.0f : 1.0f;

		// Dual scattering for multi scattering 

		float3 multiScatteredLuminance = 0.0f;
#ifdef MULTISCATAPPROX_ENABLED
		multiScatteredLuminance = GetMultipleScattering(Atmosphere, medium.scattering, medium.extinction, P, SunZenithCosAngle);
#endif

		float shadow = 1.0f;
#ifdef SHADOWMAP_ENABLED
		// First evaluate opaque shadow
		shadow = getShadow(Atmosphere, P);
#endif

		float3 S = globalL * (earthShadow * shadow * TransmittanceToSun * PhaseTimesScattering + multiScatteredLuminance * medium.scattering);
		
		float3 MS = medium.scattering * 1;
		float3 MSint = (MS - MS * SampleTransmittance) / medium.extinction;
		result.MultiScatAs1 += throughput * MSint;

		// Evaluate input to multi scattering 
		{
			float3 newMS;

			newMS = earthShadow * TransmittanceToSun * medium.scattering * uniformPhase * 1;
			result.NewMultiScatStep0Out += throughput * (newMS - newMS * SampleTransmittance) / medium.extinction;
			//	result.NewMultiScatStep0Out += SampleTransmittance * throughput * newMS * dt;

			newMS = medium.scattering * uniformPhase * multiScatteredLuminance;
			result.NewMultiScatStep1Out += throughput * (newMS - newMS * SampleTransmittance) / medium.extinction;
			//	result.NewMultiScatStep1Out += SampleTransmittance * throughput * newMS * dt;
		}

		// See slide 28 at http://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/ 
		float3 Sint = (S - S * SampleTransmittance) / medium.extinction;	// integrate along the current step segment 
		L += throughput * Sint;														// accumulate and also take into account the transmittance from previous steps
		throughput *= SampleTransmittance;

#endif // OPTICAL_DEPTH_ONLY

	}

	if (ground && tMax == tBottom && tBottom > 0.0)
	{
		// Account for bounced light off the earth
		float3 P = WorldPos + tBottom * WorldDir;
		float pHeight = length(P);

		const float3 UpVector = P / pHeight;
		float SunZenithCosAngle = dot(SunDir, UpVector);
		float2 uv;
		LutTransmittanceParamsToUv(Atmosphere, pHeight, SunZenithCosAngle, uv);

		SamplerState linearClampSampler = theSamplers[myLinearClampSamplerIndex];
		float3 TransmittanceToSun = theTextures2D[myTransmittanceLutTextureIndex].SampleLevel(linearClampSampler, uv, 0).rgb;

		const float NdotL = saturate(dot(normalize(UpVector), normalize(SunDir)));
		L += globalL * TransmittanceToSun * throughput * NdotL * Atmosphere.GroundAlbedo / PI;
	}

	result.L = L;
	result.OpticalDepth = OpticalDepth;
	result.Transmittance = throughput;
	return result;
}

[numthreads(8, 8, 1)]
void ComputeTransmittanceLut(uint3 aDTid : SV_DispatchThreadID)
{
	float2 pixPos = aDTid.xy + 0.5;
	AtmosphereParameters Atmosphere = myAtmosphereParameters;

	// Compute camera position from LUT coords
	float2 uv = (pixPos) / float2(TRANSMITTANCE_TEXTURE_WIDTH, TRANSMITTANCE_TEXTURE_HEIGHT);
	float viewHeight;
	float viewZenithCosAngle;
	UvToLutTransmittanceParams(Atmosphere, viewHeight, viewZenithCosAngle, uv);

	//  A few extra needed constants
	float3 WorldPos = float3(0.0f, viewHeight, 0.0f);
	float3 WorldDir = float3(0.0f, viewZenithCosAngle, sqrt(1.0 - viewZenithCosAngle * viewZenithCosAngle));

	const bool ground = false;
	const float SampleCountIni = 40.0f;	// Can go a low as 10 sample but energy lost starts to be visible.
	const float DepthBufferValue = -1.0;
	const bool VariableSampleCount = false;
	const bool MieRayPhase = false;
	float3 transmittance = exp(-IntegrateScatteredLuminance(pixPos, WorldPos, WorldDir, sun_direction, Atmosphere, ground, SampleCountIni, DepthBufferValue, VariableSampleCount, MieRayPhase).OpticalDepth);

	// Optical depth to transmittance
	theRwTextures2D[myOutTexIndex][aDTid.xy] = float4(transmittance, 1.0);	
}

[numthreads(8, 8, 1)]
void ComputeSkyViewLut(uint3 aDTid : SV_DispatchThreadID)
{
	float2 pixPos = aDTid.xy + 0.5;
	AtmosphereParameters Atmosphere = myAtmosphereParameters;

	float3 ClipSpace = float3((pixPos / float2(SKY_VIEW_TEXTURE_WIDTH, SKY_VIEW_TEXTURE_HEIGHT)) * float2(2.0, -2.0) - float2(1.0, -1.0), 1.0);
	float4 HPos = mul(gSkyInvViewProjMat, float4(ClipSpace, 1.0));

	float3 WorldDir = normalize(HPos.xyz / HPos.w - camera);
	float3 WorldPos = camera + float3(0, Atmosphere.BottomRadius, 0);

	float2 uv = pixPos / float2(SKY_VIEW_TEXTURE_WIDTH, SKY_VIEW_TEXTURE_HEIGHT);

	float viewHeight = length(WorldPos);

	float viewZenithCosAngle;
	float lightViewCosAngle;
	UvToSkyViewLutParams(Atmosphere, viewZenithCosAngle, lightViewCosAngle, viewHeight, uv);

	float3 SunDir;
	{
		float3 UpVector = WorldPos / viewHeight;
		float sunZenithCosAngle = dot(UpVector, sun_direction);
		SunDir = normalize(float3(sqrt(1.0 - sunZenithCosAngle * sunZenithCosAngle), sunZenithCosAngle, 0.0));
	}

	WorldPos = float3(0.0f, viewHeight, 0.0f);

	float viewZenithSinAngle = sqrt(1 - viewZenithCosAngle * viewZenithCosAngle);
	WorldDir = float3(
		viewZenithSinAngle * lightViewCosAngle,
		viewZenithCosAngle,
		viewZenithSinAngle * sqrt(1.0 - lightViewCosAngle * lightViewCosAngle));

	// Move to top atmospehre
	if (!MoveToTopAtmosphere(WorldPos, WorldDir, Atmosphere.TopRadius))
	{
		// Ray is not intersecting the atmosphere
		theRwTextures2D[myOutTexIndex][aDTid.xy] = float4(0.0, 0.0, 0.0, 1.0);
	}
	else
	{
		const bool ground = false;
		const float SampleCountIni = 30;
		const float DepthBufferValue = -1.0;
		const bool VariableSampleCount = true;
		const bool MieRayPhase = true;
		SingleScatteringResult ss = IntegrateScatteredLuminance(pixPos, WorldPos, WorldDir, SunDir, Atmosphere, ground, SampleCountIni, DepthBufferValue, VariableSampleCount, MieRayPhase);

		float3 L = ss.L;

		theRwTextures2D[myOutTexIndex][aDTid.xy] = float4(L * 10, 1.0);
	}
}