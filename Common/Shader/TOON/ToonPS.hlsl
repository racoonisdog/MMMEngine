#include "../CommonSharedPS.hlsli"

cbuffer DitherParams : register(b10)
{
    float mDitherAlpha;
    float3 _ditherPadding;
}

Texture2D _lutMap : register(t10);
Texture2D _roughness : register(t11);
Texture2D _ambientOcclusion : register(t12);

// PBR Ambient
TextureCube _specular : register(t20);
TextureCube _irradiance : register(t21);
Texture2D _brdflut : register(t22);

cbuffer ToonMatBuffer : register(b3)
{
    float4 mBaseColor;

    float mAoStrength;
    float mDiffuseStr;
    float mSpecularStr;
    float mRoughness;

    float mLowLut;
    float mDiffGradientDistHalf;
    float mDiffGradientDepth;
    float mRimLightStr;

    float mEmissive;
    float3 mPadding;
}

// 수동 PCF
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

    float bias = 0.0001f;
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
    float3 ambient = mAoStrength * mLightColor;	

	 // Normal
    float3 N = normalize(input.Norm);
    float3 T = normalize(input.Tan);
    float3 B = normalize(input.BiTan);

    float3 normalMap = _normal.Sample(_sp0, input.Tex).xyz;
    normalMap = normalize(normalMap * 2.0f - 1.0f);
    float3x3 tbn = float3x3(T, B, N);
    
    N = normalize(mul(normalMap, tbn));

    float3 I = normalize(input.W_Pos.xyz - mCamPos.xyz);
    float3 R = reflect(I, N); // 큐브맵 반사를 위한 리플렉트 벡터
    float3 L = -mLightDir.xyz;
    float RimNdotL = dot(L * 1.25, N);
    float NdotL = saturate(dot(N, L));
    
    // 조명 위치와 픽셀 위치
    //float3 toLight =  - input.WorldPos;
    //float distance = length(toLight);

    // 감쇠 계수 (1 / d² 형태)
    //float attenuation = 1.0f / (distance * distance);
    
    //float lightDist = attenuation;
    
    float shadow = CalculateShadowPCF(input.S_Pos); // 그림자 인자 계산
    float shadowLut = _lutMap.Sample(_samPoint, float2(shadow * 0.5f + 0.495f, 0.5f)).r;

    float diff = NdotL;
	
	//float bandLevel = 1.0f;
	//diff = ceil(diff * bandLevel)/bandLevel;
    float diffLut = _lutMap.Sample(_samPoint, float2((diff * 0.5f) + 0.495f, 0.5f)).r; // 카툰렌더링 활성화
	

    float diffShadow = min(shadowLut, diffLut); // 그림자와 조명값 중 작은 값을 사용
    diffShadow = diffShadow > mLowLut ? diffShadow : (dot(N, -L) * mDiffGradientDistHalf + mDiffGradientDepth);

    float3 diffuse = mDiffuseStr * mLightColor * mIntensity * diffShadow;
    
    float3 viewDir = normalize(input.W_Pos.xyz - mCamPos.xyz);
    float3 halfDir = normalize(viewDir + L); // 스펙큘러연산을 위한 하프 벡터
    
    float specTex = _roughness.Sample(_sp0, input.Tex).r;
    float spec = pow(saturate(dot(halfDir, N)), 512) * sqrt(diff); // * sqrt(diff) <- 이걸 쓰면 shininess < 32 에서 아티팩트가 사라짐..!!! 
    
    spec = smoothstep(0.01, 0.02f, spec); // <- 카툰렌더링용 스펙큘러
    shadowLut = shadowLut > 0.999f ? 1.0f : 0.0f; // 카툰렌더링용 그림자
    shadowLut = diff > 0.6f ? shadowLut : 0.0f; // 디퓨즈가 낮으면 그림자 제거
    float3 specular = mSpecularStr * specTex * spec * mLightColor * shadowLut;
    
    float4 emmisive = _emissive.Sample(_sp0, input.Tex) * mEmissive;

    // 알파 클리핑용 디퓨즈 샘플링
    float4 baseTex = _albedo.Sample(_sp0, input.Tex) * mBaseColor;

    // 알파 임계값 설정 (0.1~0.5 정도 보통 사용)
    const float alphaCutoff = 0.5f;

    // 알파가 낮으면 픽셀 폐기
    clip(baseTex.a - alphaCutoff);

    float minusRimNdotL = saturate(dot(-(L * 1.25).xyz, N));

	// 림 라이트
    float _RimAmount = 0.716;
    float rimDot = 1 - dot(viewDir, N);
    float rimIntensity = rimDot * RimNdotL;
    rimIntensity = smoothstep(_RimAmount - 0.01, _RimAmount + 0.01, rimIntensity);


    float _negativeRimAmount = 0.58;
    float rimMinusIntensity = rimDot * minusRimNdotL;
    rimMinusIntensity = smoothstep(_negativeRimAmount - 0.21, _negativeRimAmount + 0.21, rimMinusIntensity);

    float3 minusRim = rimMinusIntensity * mLightColor * 0.35 * mRimLightStr;
	
    float3 rim = rimIntensity * mLightColor * mRimLightStr;
    
    float3 baseRGB = (specular + diffuse + ambient + rim).rgb * baseTex.rgb + emmisive.rgb;

	//float3 toGradientPos = gradientPos - input.WorldPos;
	//float GradientDistance = length(toGradientPos);
	//GradientDistance = max(GradientDistance, 0.0001f);
	//float GradientAttenuation = 1.0f / (GradientDistance);
	//
	//GradientAttenuation *= 3.5f * gradientIntensity; 
	//GradientAttenuation = saturate(GradientAttenuation);
	//
	//baseRGB = baseRGB * GradientAttenuation;
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

    return float4(baseRGB, baseTex.a);

}
