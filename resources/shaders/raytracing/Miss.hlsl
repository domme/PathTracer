#include "Common.hlsl"
#include "../sky/Common.hlsl"

[shader("miss")]
void Miss(inout HitInfo payload : SV_RayPayload)
{
	

    payload.colorAndDistance += float4(skyLuminance, -1.f);
}