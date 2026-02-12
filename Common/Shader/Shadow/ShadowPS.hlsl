Texture2D    _Albedo    : register(t0);   // 알베도 텍스처
SamplerState _Sampler   : register(s0);


// MeshRenderer dithering: shadow must match main pass so dithered mesh doesn't cast full shadow
cbuffer DitherParams : register(b10)
{
    float mDitherAlpha;
    float3 _ditherPadding;
};


struct PSInput
{
    float4 pos      : SV_POSITION;   // 라이트 뷰-프로젝션 좌표
    float2 texcoord : TEXCOORD0;     // UV 좌표
    float4 W_Pos : TEXCOORD2;
};

float4 main(PSInput input) : SV_TARGET
{
    // 알베도 텍스처 샘플링
    float alpha = _Albedo.Sample(_Sampler, input.texcoord).a;

    // 알파 테스트 (cutout)
    // threshold는 필요에 따라 0.5f 등으로 조정
    clip(alpha - 0.5f);

    // 디더링: 메인 패스와 동일한 Bayer 패턴 적용 — 디더링된 메쉬는 그림자도 디더링
    if (mDitherAlpha < 1.0)
    {
        const float bayer4x4[16] = { 0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0 };
        uint2 px = (uint2)input.pos.xy;
        uint idx = (px.x % 4u) + (px.y % 4u) * 4u;
        float bayerVal = (bayer4x4[idx] + 0.5) / 16.0;
        if (mDitherAlpha <= bayerVal)
            discard;
    }

    // 쉐도우맵은 깊이만 기록하므로 색상 출력은 필요 없음
    // SV_DEPTH를 쓰는 경우에는 float depth 반환
    // 여기서는 단순히 0 반환 (RTV가 없고 DSV에만 기록됨)
    return float4(0,0,0,0);
}
