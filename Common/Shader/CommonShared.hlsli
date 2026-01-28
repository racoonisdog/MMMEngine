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

// 메시 트랜스폼 버퍼 1~4
cbuffer Transbuffer : register(b1)
{
    matrix mWorld;
    matrix mNormalMatrix;
}

cbuffer BoneWorldBuffer : register(b2)
{
    matrix mBoneMat[256];
}

cbuffer BoneOffSetBuffer : register(b3)
{
    matrix mBoneOffsetMat[256];
}


// 라이트관련 5~9
// Directional Light
cbuffer LightBuffer : register(b5)
{
    float4 mLightDir[4];
    float4 mLightColor[4];
}

// 쉐도우 버퍼
cbuffer ShadowVP : register(b7)
{
    matrix mShadowView;
    matrix mShadowProjection;
}


// PP 버퍼 10~14
cbuffer ToneBuffer : register(b10)
{
    float mExposure;
    float mBrightness;
    float2 _TonePadding;
}

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

struct VS_INPUT
{
    float3 Pos : POSITION;
    float3 Norm : NORMAL;
    float3 Tan : TANGENT;
    float2 Tex : TEXCOORD0;
    int4 BoneIdx : BONEINDEX;
    float4 BoneWeight : BONEWEIGHT;
};
