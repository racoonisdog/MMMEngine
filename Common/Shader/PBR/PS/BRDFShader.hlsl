// 픽셀 셰이더(쉐이더/셰이더).
#include "../../CommonSharedPS.hlsli"
#include "../PBRShared.hlsli"

// MeshRenderer dithering (b10: same slot as particle alpha, used only in R_GEOMETRY)
cbuffer DitherParams : register(b10)
{
    float mDitherAlpha;
    float3 _ditherPadding;
}

static const float PI = 3.14159265359f;
static const float EPS = 1e-6f;

float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0) - F0)
                * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float ndfGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = max(denom, EPS);
    denom = PI * denom * denom;
    return a2 / denom;
}

float geometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    denom = max(denom, EPS);
	
    return num / denom;
}

float geometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

float CalculateShadowPCF(float4 LightPos)
{
    float currentShadowDepth = LightPos.z / LightPos.w; // 쉐도우맵 기준 NDC Z좌표
    float2 shadowUV = LightPos.xy / LightPos.w;

    shadowUV.y *= -1.0f;
    shadowUV = (shadowUV * 0.5f) + 0.5f;

    if (shadowUV.x < 0.0f || shadowUV.x > 1.0f ||
    shadowUV.y < 0.0f || shadowUV.y > 1.0f)
        return 1.0f;

    if (currentShadowDepth < 0.0f || currentShadowDepth > 1.0f)
        return 1.0f;

    float bias = 0.0002f;
    currentShadowDepth -= bias;

// 3x3 PCF
    float shadow = 0.0f;
    float2 texelSize = 1.0f / 4096.0f; // Shadow Map 크기

    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float2 offset = float2(x, y) * texelSize;
            shadow += _shadowmap.SampleCmpLevelZero(_cmpsp0, shadowUV + offset, currentShadowDepth);
        }
    }
    shadow /= 9.0f;

    return shadow;
}

float4 main(PS_INPUT input) : SV_TARGET
{
    // 알파테스트
    float4 texColor = _albedo.Sample(_sp0, input.Tex);
    clip(texColor.a - 0.5f);
    float4 baseColor = texColor * mBaseColor;
    
     // 필요한 변수를 모두 구하기
    float3 lightColor = mLightColor * mIntensity;

    float3 WorldPos = input.W_Pos;
    float4 toDirLightViewPos = input.S_Pos;

     // Normal
    float3 normalMap = _normal.Sample(_sp0, input.Tex).xyz;
    normalMap = normalize(normalMap * 2.0f - 1.0f);
    float3x3 tbn = float3x3(normalize(input.Tan), normalize(input.BiTan), normalize(input.Norm));
    
    float3 N = normalize(mul(normalMap, tbn));

    float3 V = normalize(mCamPos.xyz - WorldPos.xyz);
    float3 L = normalize(-mLightDir.xyz);
    float3 H = normalize(V + L);
    float3 R = reflect(-V, N);

    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));

    float4 albedo = baseColor;
    float metallic = _metallic.Sample(_sp0, input.Tex).r * mMetallic;
    float roughness = _roughness.Sample(_sp0, input.Tex).r * mRoughness;
    float ao = _ambientOcclusion.Sample(_sp0, input.Tex).r * 0.5f;

    // BRDF 계산
    // Cook-Torrance 모델

    // 직접광
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo.rgb, metallic);
    float NDF = ndfGGX(N, H, roughness);
    float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    float G = geometrySmith(N, V, L, roughness);

    float3 numerator = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL + 1e-4f;
    float3 specularBRDF = numerator / denominator;
    float3 kS = F;
    float3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;
    float3 diffuseBRDF = (kD * albedo.rgb / PI);
    float3 Lo = (diffuseBRDF + specularBRDF) * lightColor * NdotL;

    float shadow = CalculateShadowPCF(toDirLightViewPos);
    Lo *= shadow;

    // 간접광 (IBL)
    float3 irradiance = _irradiance.Sample(_sp0, N).rgb;
    float3 F_ibl = fresnelSchlickRoughness(NdotV, F0, roughness);
    float3 kD_ibl = (1.0 - F_ibl) * (1.0 - metallic);
    float3 diffuseIBL = kD_ibl * irradiance * albedo.rgb;
    
    uint width, height, numMips;
    _specular.GetDimensions(0, width, height, numMips);

    float maxMip = float(numMips - 1);
    float lod = roughness * maxMip;

    float3 prefilteredColor = _specular.SampleLevel(_sp0, R, lod).rgb;
    float2 brdf = _brdflut.Sample(_sp0, float2(NdotV, roughness)).rg;
    float3 specularIBL = prefilteredColor * (F_ibl * brdf.x + brdf.y);

    float3 ambient = (diffuseIBL + specularIBL) * ao;
    
    float3 finalColor = Lo + ambient;

    // Dithering: screen-space 8x8 Bayer (smooth, no world-grid blocky artifact)
    if (mDitherAlpha < 1.0)
    {
        const float bayer8x8[64] = { 0.0,32.0,8.0,40.0,2.0,34.0,10.0,42.0, 48.0,16.0,56.0,24.0,50.0,18.0,58.0,26.0, 12.0,44.0,4.0,36.0,14.0,46.0,6.0,38.0, 60.0,28.0,52.0,20.0,62.0,30.0,54.0,22.0, 3.0,35.0,11.0,43.0,1.0,33.0,9.0,41.0, 51.0,19.0,59.0,27.0,49.0,17.0,57.0,25.0, 15.0,47.0,7.0,39.0,13.0,45.0,5.0,37.0, 63.0,31.0,55.0,23.0,61.0,29.0,53.0,21.0 };
        uint2 px = (uint2)input.Pos.xy;
        uint idx = (px.x % 8u) + (px.y % 8u) * 8u;
        float bayerVal = (bayer8x8[idx] + 0.5) / 64.0;
        if (mDitherAlpha <= bayerVal)
            discard;
    }

    return float4(finalColor, baseColor.a);
}
