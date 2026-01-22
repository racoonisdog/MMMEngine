#include "../../CommonShared.hlsli"
#include "../../DeferredShared.hlsli"
#include "../PBRShared.hlsli"

struct PS_GOUTPUT
{
    float4 albedo   : SV_Target0;
    float4 arm      : SV_Target1;
    float4 normal   : SV_Target2;
    float4 pos      : SV_Target3;
};

PS_GOUTPUT main(PS_INPUT input)
{
    PS_GOUTPUT _out;
    
    // Albedo
    float4 texColor = mUseOverride ? mBaseColor : _albedo.Sample(_sp0, input.Tex);
    clip(texColor.a - 0.5f);
    _out.albedo = texColor;
    
    // ARM
    float ao = mUseOverride ? mAoStrength : _ambientOcclusion.Sample(_sp0, input.Tex).r * mAoStrength;
    float roughness = mUseOverride ? mRoughness : _roughness.Sample(_sp0, input.Tex).r;
    float metalic = mUseOverride ? mMetalic : _metalic.Sample(_sp0, input.Tex).r;
    _out.arm = float4(ao, roughness, metalic, 1.0f);
    
    // Normal
    float3 normalMap = _normal.Sample(_sp0, input.Tex).xyz;
    normalMap = normalize(normalMap * 2.0f - 1.0f);
    float3x3 tbn = float3x3(normalize(input.Tan), normalize(input.BiTan), normalize(input.Norm));
    
    float3 worldNormal = normalize(mul(normalMap, tbn));
    _out.normal = float4(worldNormal * 0.5f + 0.5f, 1.0f);
    
    // Pos
    _out.pos = input.W_Pos;
    
    return _out;
}