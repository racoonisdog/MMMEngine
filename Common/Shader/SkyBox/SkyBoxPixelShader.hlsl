#include "SkyBoxShared.hlsli"

float4 main(VS_SKYOUT input) : SV_TARGET
{
    return _cubemap.Sample(_sample, input.Dir);
}