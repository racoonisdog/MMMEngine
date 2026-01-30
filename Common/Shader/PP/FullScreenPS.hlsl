// 풀스크린 트라이앵글 PS
Texture2D _inputTex : register(t0);
SamplerState _sp0  : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    // 텍스처 샘플링
    float4 color = _inputTex.Sample(_sp0, input.uv);

    // 간단한 효과 예시: 색 반전
    // color = float4(1.0 - color.rgb, color.a);

    return color;
}
