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

cbuffer ShadowVP : register(b4)
{
    matrix mShadowView;
    matrix mShadowProjection;
}

struct VS_OUTPUT
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
