#ifndef INC_SKY_ATMOSPHERE_COMMMON
#define INC_SKY_ATMOSPHERE_COMMMON

#include "fancy/resources/shaders/GlobalResources.h"

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

cbuffer SKYATMOSPHERE_BUFFER : register(b0, Space_LocalCBuffer)
{
	AtmosphereParameters myAtmosphereParameters;

	int TRANSMITTANCE_TEXTURE_WIDTH;
	int TRANSMITTANCE_TEXTURE_HEIGHT;
	int IRRADIANCE_TEXTURE_WIDTH;
	int IRRADIANCE_TEXTURE_HEIGHT;
	
	int SCATTERING_TEXTURE_R_SIZE;
	int SCATTERING_TEXTURE_MU_SIZE;
	int SCATTERING_TEXTURE_MU_S_SIZE;
	int SCATTERING_TEXTURE_NU_SIZE;

	float3 SKY_SPECTRAL_RADIANCE_TO_LUMINANCE;
	int SKY_VIEW_TEXTURE_WIDTH;
	float3 SUN_SPECTRAL_RADIANCE_TO_LUMINANCE;
	int SKY_VIEW_TEXTURE_HEIGHT;

	//
	// Other globals
	//
	float4x4 gSkyViewProjMat;
	float4x4 gSkyInvViewProjMat;
	float4x4 gShadowmapViewProjMat;

	float3 camera;
	float  pad5;
	float3 sun_direction;
	float  pad6;
	float3 view_ray;
	float  pad7;

	float MultipleScatteringFactor;
	float MultiScatteringLUTRes;
	float pad9;
	float pad10;
};

/*
AtmosphereParameters GetAtmosphereParameters()
{
	AtmosphereParameters Parameters;
	Parameters.AbsorptionExtinction = absorption_extinction;

	// Traslation from Bruneton2017 parameterisation.
	Parameters.RayleighDensityExpScale = rayleigh_density[1].w;
	Parameters.MieDensityExpScale = mie_density[1].w;
	Parameters.AbsorptionDensity0LayerWidth = absorption_density[0].x;
	Parameters.AbsorptionDensity0ConstantTerm = absorption_density[1].x;
	Parameters.AbsorptionDensity0LinearTerm = absorption_density[0].w;
	Parameters.AbsorptionDensity1ConstantTerm = absorption_density[2].y;
	Parameters.AbsorptionDensity1LinearTerm = absorption_density[2].x;

	Parameters.MiePhaseG = mie_phase_function_g;
	Parameters.RayleighScattering = rayleigh_scattering;
	Parameters.MieScattering = mie_scattering;
	Parameters.MieAbsorption = mie_absorption;
	Parameters.MieExtinction = mie_extinction;
	Parameters.GroundAlbedo = ground_albedo;
	Parameters.BottomRadius = bottom_radius;
	Parameters.TopRadius = top_radius;
	return Parameters;
}
*/

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

void LutTransmittanceParamsToUv(AtmosphereParameters Atmosphere, in float viewHeight, in float viewZenithCosAngle, out float2 uv)
{
	float H = sqrt(max(0.0f, Atmosphere.TopRadius * Atmosphere.TopRadius - Atmosphere.BottomRadius * Atmosphere.BottomRadius));
	float rho = sqrt(max(0.0f, viewHeight * viewHeight - Atmosphere.BottomRadius * Atmosphere.BottomRadius));

	float discriminant = viewHeight * viewHeight * (viewZenithCosAngle * viewZenithCosAngle - 1.0) + Atmosphere.TopRadius * Atmosphere.TopRadius;
	float d = max(0.0, (-viewHeight * viewZenithCosAngle + sqrt(discriminant))); // Distance to atmosphere boundary

	float d_min = Atmosphere.TopRadius - viewHeight;
	float d_max = rho + H;
	float x_mu = (d - d_min) / (d_max - d_min);
	float x_r = rho / H;

	uv = float2(x_mu, x_r);
	//uv = float2(fromUnitToSubUvs(uv.x, TRANSMITTANCE_TEXTURE_WIDTH), fromUnitToSubUvs(uv.y, TRANSMITTANCE_TEXTURE_HEIGHT)); // No real impact so off
}

#endif