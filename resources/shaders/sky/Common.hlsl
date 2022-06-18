#ifndef INC_SKY_COMMON
#define INC_SKY_COMMON

#include "fancy/resources/shaders/GlobalResources.h"
#include "fancy/resources/shaders/common_types.h"

cbuffer CONSTANT_BUFFER : register(b1, Space_LocalCBuffer)
{
	float3 gSunIlluminance;
	int gScatteringMaxPathDepth;

	uint2 gResolution;
	float gFrameTimeSec;
	float gTimeSec;

	float2 RayMarchMinMaxSPP;
	uint gFrameId;

	uint myOutTexIndex;
	uint myTransmittanceLutTextureIndex;
	uint myLinearClampSamplerIndex;
};

#endif