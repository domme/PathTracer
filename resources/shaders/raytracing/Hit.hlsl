#include "Common.hlsl"

[shader("closesthit")] 
void ClosestHit(inout HitInfo payload, Attributes attrib) 
{
  uint instanceId = InstanceID();
  payload.colorAndDistance = float4(instanceId / 10.0, instanceId / 10.0, instanceId / 10.0, RayTCurrent());
}
