#include "../../CommonShared.hlsli"
#include "../../DeferredShared.hlsli"
#include "../PBRShared.hlsli"

// 픽셀 셰이더(쉐이더/셰이더).
struct PS_DEFINPUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

// GGX
float ndfGGX(float3 N, float3 H, float alpha)
{
    float NH = saturate(dot(N, H));
    float a2 = alpha * alpha;
    float d = (NH * NH) * (a2 - 1.0f) + 1.0f;
    return a2 / (3.141592f * (d * d));
}

// Fresnel
float3 Fresnel(float3 H, float3 V, float3 F0)
{
    float HV = saturate(dot(H, V));
    return F0 + (1.0f - F0) * pow(1.0f - HV, 5.0f);
}

// GeoSchlickSub
float GeoSchlickSub(float NdotX, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) * 0.125f;
    return NdotX / (NdotX * (1.0f - k) + k);
}

// GeoSchlick
float GeoSchlick(float NV, float NL, float roughness)
{
    
    float gv = GeoSchlickSub(NV, roughness);
    float gl = GeoSchlickSub(NL, roughness);

    return gv * gl;
}

float4 main(PS_DEFINPUT input) : SV_TARGET
{   
    // 위치 샘플링
    float3 W_Pos = _defPos.Sample(_sp0, input.uv).rgb;
    float4 S_Pos = mul(float4(W_Pos.xyz, 1.0f), ShadowView);;
    S_Pos = mul(S_Pos, ShadowProjection);
    
    // 텍스처 샘플링
    float3 albedo = _defAlbedo.Sample(_sp0, input.uv).rgb;
    float metalic = _defARM.Sample(_sp0, input.uv).b;
    float roughness = _defARM.Sample(_sp0, input.uv).g;
    float ao = _defARM.Sample(_sp0, input.uv).r;
    
    // 노멀
    float3 normalMap = _defNormal.Sample(_sp0, input.uv).xyz;
    
    // 벡터
    float3 N = normalize(normalMap * 2.0f - 1.0f);
    float3 V = normalize(mCamPos.xyz - W_Pos.xyz);
    float3 L = normalize(-mLightDir.xyz);
    float3 H = normalize(V + L);
    
    // 기본반사율 구하기
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metalic);
    
    //BRDF 구성요소

    float NV = saturate(dot(N, V));
    float NL = saturate(dot(N, L));
    
    float alpha = max(roughness * roughness, 0.001f);
    float D = ndfGGX(N, H, alpha);
    float3 F = Fresnel(H, V, F0);
    float G = GeoSchlick(NV, NL, roughness);
    
    float3 kd = lerp(1.0f - F, 0.0f, metalic);
    float3 diffuse = (kd * albedo / 3.141592f);
    float3 specular = (D * F * G) / max(4.0f * NL * NV, 0.001f);
    
    // 광량 샘플링
    float3 irradiance = _irradiance.Sample(_sp0, N).rgb;
    float3 diffuseIBL = saturate(kd * albedo * irradiance);
    
    // LOD Mipmap 레벨 얻기
    int specularTextureLevels, width, height;
    _specular.GetDimensions(0, width, height, specularTextureLevels);
    // 반사 샘플링
    float3 Lr = reflect(-V, N);
    float3 PrefilteredColor = _specular.SampleLevel(_sp0, Lr, roughness * specularTextureLevels).rgb;
    
    // BRDF 샘플링
    float2 specularBRDF = _brdflut.Sample(_sp0, float2(NV, roughness)).rg;
    float3 specularIBL = PrefilteredColor * (F0 * specularBRDF.r + specularBRDF.g);
   
    float3 amibentIBL = (diffuseIBL + specularIBL) * ao;
    
    // 쉐도우맵 처리
    float currentShadowDepth = S_Pos.z / S_Pos.w; // 쉐도우맵 기준 NDC Z좌표
    float2 shadowUV = S_Pos.xy / S_Pos.w;
    
    shadowUV.y *= -1.0f;
    shadowUV = (shadowUV * 0.5f) + 0.5f;
    
    float shadowFactor = 1.0f;
    
    if (shadowUV.x >= 0.0f && shadowUV.x <= 1.0f && shadowUV.y >= 0.0f && shadowUV.y <= 1.0f)
    {
        // Normal
        {
            float sampleShadowDepth = _shadowmap.Sample(_sp0, shadowUV).r;
        
            if (currentShadowDepth > sampleShadowDepth + 0.001f)
            {
                shadowFactor = 0.0f;
            }
        }
    }
    
    float3 light = mLightColor.rgb;
    float3 direct = (diffuse + specular) * light * NL;
    float3 color = (direct * shadowFactor) + amibentIBL;
    //float4 finalColor = float4(pow(color, 1.0f / 2.2f), 1.0f);
    
    return float4(color, 1.0f);
    //return float4(N, 1.0f);
}