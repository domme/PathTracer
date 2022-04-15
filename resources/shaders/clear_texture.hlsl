#include "fancy/resources/shaders/GlobalResources.h"

cbuffer CB0 : register(b0, Space_LocalCBuffer)
{
	uint myTexIdx;
};

[numthreads(8, 8, 1)]
void main(uint3 aDTid : SV_DispatchThreadID)
{
    theRwTextures2D[myTexIdx][aDTid.xy] = float4(0, 0, 0, 0);
}
