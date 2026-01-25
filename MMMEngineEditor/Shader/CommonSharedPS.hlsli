// 공용 1~19
Texture2D _albedo : register(t0);
Texture2D _normal: register(t1);
Texture2D _emissive : register(t2);
Texture2D _shadowmap : register(t3);
Texture2D _opacity : register(t4);

SamplerState _sp0 : register(s0);

// 카메라버퍼
cbuffer Cambuffer : register(b0)
{
    matrix mView;
    matrix mProjection;
    float4 mCamPos;
}

// Directional Light 1~2
cbuffer LightBuffer : register(b1)
{
    float4 mLightDir;
    float3 mLightColor;
    float  mIntensity;
}

// PP 버퍼 10~14
// cbuffer ToneBuffer : register(b10)
// {
//     float mExposure;
//     float mBrightness;
//     float2 _TonePadding;
// }

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float3 Norm : NORMAL;
    float3 Tan : TANGENT;
    float3 BiTan : BITANGENT;
    float2 Tex : TEXCOORD0;     // 텍스쳐 UV
    float4 S_Pos : TEXCOORD1;   // 쉐도우 포지션
    float4 W_Pos : TEXCOORD2;   // 월드 포지션
};