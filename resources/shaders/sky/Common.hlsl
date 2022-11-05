#ifndef INC_SKY_COMMON
#define INC_SKY_COMMON

#include "../../../fancy/resources/shaders/GlobalResources.h"
#include "../../../fancy/resources/shaders/common_types.h"

struct AtmosphereParameters
{
	// Rayleigh scattering coefficients
	float3 RayleighScattering;
	// Rayleigh scattering exponential distribution scale in the atmosphere
	float RayleighDensityExpScale;

	// This other medium only absorb light, e.g. useful to represent ozone in the earth atmosphere
	float3 AbsorptionExtinction;
	// Radius of the planet (center to ground)
	float BottomRadius;

	// The albedo of the ground.
	float3 GroundAlbedo;
	// Maximum considered atmosphere height (center to atmosphere top)
	float TopRadius;
	
	// Mie scattering coefficients
	float3 MieScattering;
	// Mie scattering exponential distribution scale in the atmosphere
	float MieDensityExpScale;

	// Mie extinction coefficients
	float3 MieExtinction;
	// Mie phase function excentricity
	float MiePhaseG;

	// Mie absorption coefficients
	float3 MieAbsorption;
	// Another medium type in the atmosphere
	float AbsorptionDensity0LayerWidth;

	float AbsorptionDensity0ConstantTerm;
	float AbsorptionDensity0LinearTerm;
	float AbsorptionDensity1ConstantTerm;
	float AbsorptionDensity1LinearTerm;
};

/*

cbuffer CONSTANT_BUFFER : register(b1, Space_LocalCBuffer)
{
	float3 gSunIlluminance;
	int gScatteringMaxPathDepth;

	uint2 gResolution;
	float2 RayMarchMinMaxSPP;

	uint myOutTexIndex;
	uint myTransmittanceLutTextureIndex;
	uint myLinearClampSamplerIndex;
	uint myDepthBufferIndex;

	uint mySkyViewLutTextureIndex;
	uint gFrameId;
};

*/

float fromUnitToSubUvs(float u, float resolution) { return (u + 0.5f / resolution) * (resolution / (resolution + 1.0f)); }
float fromSubUvsToUnit(float u, float resolution) { return (u - 0.5f / resolution) * (resolution / (resolution - 1.0f)); }

// - r0: ray origin
// - rd: normalized ray direction
// - s0: sphere center
// - sR: sphere radius
// - Returns distance from r0 to first intersecion with sphere,
//   or -1.0 if no intersection.
float raySphereIntersectNearest(float3 r0, float3 rd, float3 s0, float sR)
{
	float a = dot(rd, rd);
	float3 s0_r0 = r0 - s0;
	float b = 2.0 * dot(rd, s0_r0);
	float c = dot(s0_r0, s0_r0) - (sR * sR);
	float delta = b * b - 4.0*a*c;
	if (delta < 0.0 || a == 0.0)
	{
		return -1.0;
	}
	float sol0 = (-b - sqrt(delta)) / (2.0*a);
	float sol1 = (-b + sqrt(delta)) / (2.0*a);
	if (sol0 < 0.0 && sol1 < 0.0)
	{
		return -1.0;
	}
	if (sol0 < 0.0)
	{
		return max(0.0, sol1);
	}
	else if (sol1 < 0.0)
	{
		return max(0.0, sol0);
	}
	return max(0.0, min(sol0, sol1));
}

void LutTransmittanceParamsToUv(float anAtmosphereBottomRadius, float anAtmosphereTopRadius, in float viewHeight, in float viewZenithCosAngle, out float2 uv)
{
	float H = sqrt(max(0.0f, anAtmosphereTopRadius * anAtmosphereTopRadius - anAtmosphereBottomRadius * anAtmosphereBottomRadius));
	float rho = sqrt(max(0.0f, viewHeight * viewHeight - anAtmosphereBottomRadius * anAtmosphereBottomRadius));

	float discriminant = viewHeight * viewHeight * (viewZenithCosAngle * viewZenithCosAngle - 1.0) + anAtmosphereTopRadius * anAtmosphereTopRadius;
	float d = max(0.0, (-viewHeight * viewZenithCosAngle + sqrt(discriminant))); // Distance to atmosphere boundary

	float d_min = anAtmosphereTopRadius - viewHeight;
	float d_max = rho + H;
	float x_mu = (d - d_min) / (d_max - d_min);
	float x_r = rho / H;

	uv = float2(x_mu, x_r);
}

void SkyViewLutParamsToUv(float anAtmosphereBottomRadius, uint2 aSkyViewTextureSize, in bool IntersectGround, in float viewZenithCosAngle, in float lightViewCosAngle, in float viewHeight, out float2 uv)
{
	float Vhorizon = sqrt(viewHeight * viewHeight - anAtmosphereBottomRadius * anAtmosphereBottomRadius);
	float CosBeta = Vhorizon / viewHeight;				// GroundToHorizonCos
	float Beta = acos(CosBeta);
	float ZenithHorizonAngle = PI - Beta;

	if (!IntersectGround)
	{
		float coord = acos(viewZenithCosAngle) / ZenithHorizonAngle;
		coord = 1.0 - coord;
		coord = sqrt(coord);
		coord = 1.0 - coord;
		uv.y = coord * 0.5f;
	}
	else
	{
		float coord = (acos(viewZenithCosAngle) - ZenithHorizonAngle) / Beta;
		coord = sqrt(coord);
		uv.y = coord * 0.5f + 0.5f;
	}

	{
		float coord = -lightViewCosAngle * 0.5f + 0.5f;
		coord = sqrt(coord);
		uv.x = coord;
	}

	// Constrain uvs to valid sub texel range (avoid zenith derivative issue making LUT usage visible)
	uv = float2(fromUnitToSubUvs(uv.x, aSkyViewTextureSize.x), fromUnitToSubUvs(uv.y, aSkyViewTextureSize.y));
}

#define RAYDPOS 0.00001f

#define PLANET_RADIUS_OFFSET 10.0 // 0.01

////////////////////////////////////////////////////////////
// Participating media
////////////////////////////////////////////////////////////

float getAlbedo(float scattering, float extinction)
{
	return scattering / max(0.001, extinction);
}
float3 getAlbedo(float3 scattering, float3 extinction)
{
	return scattering / max(0.001, extinction);
}

struct MediumSampleRGB
{
	float3 scattering;
	float3 absorption;
	float3 extinction;

	float3 scatteringMie;
	float3 absorptionMie;
	float3 extinctionMie;

	float3 scatteringRay;
	float3 absorptionRay;
	float3 extinctionRay;

	float3 scatteringOzo;
	float3 absorptionOzo;
	float3 extinctionOzo;

	float3 albedo;
};


// aPosWorld: Position relative to the planet's center
MediumSampleRGB sampleMediumRGB(in float3 aPosWorld, in AtmosphereParameters Atmosphere)
{
	const float viewHeight = length(aPosWorld) - Atmosphere.BottomRadius;

	const float densityMie = exp(Atmosphere.MieDensityExpScale * viewHeight);
	const float densityRay = exp(Atmosphere.RayleighDensityExpScale * viewHeight);
	const float densityOzo = saturate(viewHeight < Atmosphere.AbsorptionDensity0LayerWidth ?
		Atmosphere.AbsorptionDensity0LinearTerm * viewHeight + Atmosphere.AbsorptionDensity0ConstantTerm :
		Atmosphere.AbsorptionDensity1LinearTerm * viewHeight + Atmosphere.AbsorptionDensity1ConstantTerm);

	MediumSampleRGB s;

	s.scatteringMie = densityMie * Atmosphere.MieScattering;
	s.absorptionMie = densityMie * Atmosphere.MieAbsorption;
	s.extinctionMie = densityMie * Atmosphere.MieExtinction;

	s.scatteringRay = densityRay * Atmosphere.RayleighScattering;
	s.absorptionRay = 0.0f;
	s.extinctionRay = s.scatteringRay + s.absorptionRay;

	s.scatteringOzo = 0.0;
	s.absorptionOzo = densityOzo * Atmosphere.AbsorptionExtinction;
	s.extinctionOzo = s.scatteringOzo + s.absorptionOzo;

	s.scattering = s.scatteringMie + s.scatteringRay + s.scatteringOzo;
	s.absorption = s.absorptionMie + s.absorptionRay + s.absorptionOzo;
	s.extinction = s.extinctionMie + s.extinctionRay + s.extinctionOzo;
	s.albedo = getAlbedo(s.scattering, s.extinction);

	return s;
}

////////////////////////////////////////////////////////////
// Sampling functions
////////////////////////////////////////////////////////////

// Generate a sample (using importance sampling) along an infinitely long path with a given constant extinction.
// Zeta is a random number in [0,1]
float infiniteTransmittanceIS(float extinction, float zeta)
{
	return -log(1.0f - zeta) / extinction;
}
// Normalized PDF from a sample on an infinitely long path according to transmittance and extinction.
float infiniteTransmittancePDF(float extinction, float transmittance)
{
	return extinction * transmittance;
}

// Same as above but a sample is generated constrained within a range t,
// where transmittance = exp(-extinction*t) over that range.
float rangedTransmittanceIS(float extinction, float transmittance, float zeta)
{
	return -log(1.0f - zeta * (1.0f - transmittance)) / extinction;
}

float CornetteShanksMiePhaseFunction(float g, float cosTheta)
{
	float k = 3.0 / (8.0 * PI) * (1.0 - g * g) / (2.0 + g * g);
	return k * (1.0 + cosTheta * cosTheta) / pow(1.0 + g * g - 2.0 * g * -cosTheta, 1.5);
}

float hgPhase(float g, float cosTheta)
{
	return CornetteShanksMiePhaseFunction(g, cosTheta);
}

float dualLobPhase(float g0, float g1, float w, float cosTheta)
{
	return lerp(hgPhase(g0, cosTheta), hgPhase(g1, cosTheta), w);
}

float uniformPhase()
{
	return 1.0f / (4.0f * PI);
}

float RayleighPhase(float cosTheta)
{
	float factor = 3.0f / (16.0f * PI);
	return factor * (1.0f + cosTheta * cosTheta);
}

void UvToLutTransmittanceParams(AtmosphereParameters Atmosphere, out float viewHeight, out float viewZenithCosAngle, in float2 uv)
{
	//uv = float2(fromSubUvsToUnit(uv.x, TRANSMITTANCE_TEXTURE_WIDTH), fromSubUvsToUnit(uv.y, TRANSMITTANCE_TEXTURE_HEIGHT)); // No real impact so off
	float x_mu = uv.x;
	float x_r = uv.y;

	float H = sqrt(Atmosphere.TopRadius * Atmosphere.TopRadius - Atmosphere.BottomRadius * Atmosphere.BottomRadius);
	float rho = H * x_r;
	viewHeight = sqrt(rho * rho + Atmosphere.BottomRadius * Atmosphere.BottomRadius);

	float d_min = Atmosphere.TopRadius - viewHeight;
	float d_max = rho + H;
	float d = d_min + x_mu * (d_max - d_min);
	viewZenithCosAngle = d == 0.0 ? 1.0f : (H * H - rho * rho - d * d) / (2.0 * viewHeight * d);
	viewZenithCosAngle = clamp(viewZenithCosAngle, -1.0, 1.0);
}

void UvToSkyViewLutParams(float atmosphereBottomRadius, float atmosphereTopRadius, out float viewZenithCosAngle, out float lightViewCosAngle, in float viewHeight, in float2 uv, in float2 skyViewLutRes)
{
	// Constrain uvs to valid sub texel range (avoid zenith derivative issue making LUT usage visible)
	uv = float2(fromSubUvsToUnit(uv.x, skyViewLutRes.x), fromSubUvsToUnit(uv.y, skyViewLutRes.y));

	float Vhorizon = sqrt(viewHeight * viewHeight - atmosphereBottomRadius * atmosphereBottomRadius);
	float CosBeta = Vhorizon / viewHeight; // GroundToHorizonCos
	float Beta = acos(CosBeta);
	float ZenithHorizonAngle = PI - Beta;

	if (uv.y < 0.5f)
	{
		float coord = 2.0*uv.y;
		coord = 1.0 - coord;
		coord *= coord;  // Non linear sky view lut
		coord = 1.0 - coord;
		viewZenithCosAngle = cos(ZenithHorizonAngle * coord);
	}
	else
	{
		float coord = uv.y*2.0 - 1.0;
		coord *= coord;  // non linear sky view lut
		viewZenithCosAngle = cos(ZenithHorizonAngle + Beta * coord);
	}

	float coord = uv.x;
	coord *= coord;
	lightViewCosAngle = -(coord*2.0 - 1.0);
}

bool MoveToTopAtmosphere(inout float3 aPosWorld, in float3 aaViewDir, in float AtmosphereTopRadius)
{
	float viewHeight = length(aPosWorld);
	if (viewHeight > AtmosphereTopRadius)
	{
		float tTop = raySphereIntersectNearest(aPosWorld, aaViewDir, float3(0.0f, 0.0f, 0.0f), AtmosphereTopRadius);
		if (tTop >= 0.0f)
		{
			float3 UpVector = aPosWorld / viewHeight;
			float3 UpOffset = UpVector * -PLANET_RADIUS_OFFSET;
			aPosWorld = aPosWorld + aaViewDir * tTop + UpOffset;
		}
		else
		{
			// Ray is not intersecting the atmosphere
			return false;
		}
	}
	return true; // ok to start tracing
}

// Sun disk
float3 GetSunLuminance(float3 aPosWorld, float3 aaViewDir, float3 aaSunDirection, float aPlanetRadius)
{
	if (dot(aaViewDir, aaSunDirection) > cos(0.5*0.505*3.14159 / 180.0))
	{
		float t = raySphereIntersectNearest(aPosWorld, aaViewDir, float3(0.0f, 0.0f, 0.0f), aPlanetRadius);
		if (t < 0.0f) // no intersection
		{
			const float3 SunLuminance = 1000000.0; // arbitrary. But fine, not use when comparing the models
			return SunLuminance;
		}
	}

	return float3(0,0,0);
}

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
	in float2 aClipPos, 
	in float3 aPosWorld, 
	in float3 aViewDir, 
	in float3 aSunDir, 
	in AtmosphereParameters Atmosphere,
	in bool ground, 
	in float SampleCountIni, 
	in float DepthBufferValue, 
	in bool VariableSampleCount,
	in bool MieRayPhase, 
	in float4x4 aInvViewProjMat,
	in float2 aRayMarchMinMaxSPP,
	in float3 aSunIlluminance,
	in uint aLinearClampSamplerIdx,
	in uint aTransmittanceLutTexIdx,
	in float tMaxMax = 9000000.0f)
{
	SingleScatteringResult result = (SingleScatteringResult) 0;

	// Compute next intersection with atmosphere or ground 
	float3 earthO = float3(0.0f, 0.0f, 0.0f);
	float tBottom = raySphereIntersectNearest(aPosWorld, aViewDir, earthO, Atmosphere.BottomRadius);
	float tTop = raySphereIntersectNearest(aPosWorld, aViewDir, earthO, Atmosphere.TopRadius);
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
		float3 ClipSpace = float3(aClipPos, DepthBufferValue);
		if (ClipSpace.z < 1.0f)
		{
			float4 DepthBufferWorldPos = mul(aInvViewProjMat, float4(ClipSpace, 1.0));
			DepthBufferWorldPos /= DepthBufferWorldPos.w;

			float tDepth = length(DepthBufferWorldPos.xyz - (aPosWorld + float3(0.0, -Atmosphere.BottomRadius, 0.0))); // apply earth offset to go back to origin as top of earth mode. 
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
		SampleCount = lerp(aRayMarchMinMaxSPP.x, aRayMarchMinMaxSPP.y, saturate(tMax*0.01));
		SampleCountFloor = floor(SampleCount);
		tMaxFloor = tMax * SampleCountFloor / SampleCount;	// rescale tMax to map to the last entire step segment.
	}
	float dt = tMax / SampleCount;

	// Phase functions
	const float uniformPhase = 1.0 / (4.0 * PI);
	const float3 wi = aSunDir;
	const float3 wo = aViewDir;
	float cosTheta = dot(wi, wo);
	float MiePhaseValue = hgPhase(Atmosphere.MiePhaseG, -cosTheta);	// negate cosTheta because due to aViewDir being a "in" direction. 
	float RayleighPhaseValue = RayleighPhase(cosTheta);

#ifdef ILLUMINANCE_IS_ONE
	// When building the scattering factor, we assume light illuminance is 1 to compute a transfert function relative to identity illuminance of 1.
	// This make the scattering factor independent of the light. It is now only linked to the atmosphere properties.
	float3 globalL = 1.0f;
#else
	float3 globalL = aSunIlluminance;
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
		float3 P = aPosWorld + t * aViewDir;

		MediumSampleRGB medium = sampleMediumRGB(P, Atmosphere);
		const float3 SampleOpticalDepth = medium.extinction * dt;
		const float3 SampleTransmittance = exp(-SampleOpticalDepth);
		OpticalDepth += SampleOpticalDepth;

#ifndef OPTICAL_DEPTH_ONLY

		float pHeight = length(P);
		const float3 UpVector = P / pHeight;
		float SunZenithCosAngle = dot(aSunDir, UpVector);
		float2 uv;
		LutTransmittanceParamsToUv(Atmosphere.BottomRadius, Atmosphere.TopRadius, pHeight, SunZenithCosAngle, uv);
		SamplerState linearClampSampler = theSamplers[aLinearClampSamplerIdx];
		float3 TransmittanceToSun = theTextures2D[aTransmittanceLutTexIdx].SampleLevel(linearClampSampler, uv, 0).rgb;

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
		float tEarth = raySphereIntersectNearest(P, aSunDir, earthO + PLANET_RADIUS_OFFSET * UpVector, Atmosphere.BottomRadius);
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
		float3 P = aPosWorld + tBottom * aViewDir;
		float pHeight = length(P);

		const float3 UpVector = P / pHeight;
		float SunZenithCosAngle = dot(aSunDir, UpVector);
		float2 uv;
		LutTransmittanceParamsToUv(Atmosphere.BottomRadius, Atmosphere.TopRadius, pHeight, SunZenithCosAngle, uv);

		SamplerState linearClampSampler = theSamplers[aLinearClampSamplerIdx];
		float3 TransmittanceToSun = theTextures2D[aTransmittanceLutTexIdx].SampleLevel(linearClampSampler, uv, 0).rgb;

		const float NdotL = saturate(dot(normalize(UpVector), normalize(aSunDir)));
		L += globalL * TransmittanceToSun * throughput * NdotL * Atmosphere.GroundAlbedo / PI;
	}

	result.L = L;
	result.OpticalDepth = OpticalDepth;
	result.Transmittance = throughput;
	return result;
}

#endif