#ifndef INC_RENDER_SKY_COMMON
#define INC_RENDER_SKY_COMMON

#include "fancy/resources/shaders/GlobalResources.h"
#include "SkyAtmosphereCommon.hlsl"

#define RAYDPOS 0.00001f

#define PLANET_RADIUS_OFFSET 0.01

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

MediumSampleRGB sampleMediumRGB(in float3 WorldPos, in AtmosphereParameters Atmosphere)
{
	const float viewHeight = length(WorldPos) - Atmosphere.BottomRadius;

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

// Generates a uniform distribution of directions over a sphere.
// Random zetaX and zetaY values must be in [0, 1].
// Top and bottom sphere pole (+-zenith) are along the Y axis.
float3 getUniformSphereSample(float zetaX, float zetaY)
{
	float phi = 2.0f * 3.14159f * zetaX;
	float theta = 2.0f * acos(sqrt(1.0f - zetaY));
	float3 dir = float3(sin(theta)*cos(phi), cos(theta), sin(theta)*sin(phi));
	return dir;
}

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

#endif