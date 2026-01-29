TextureCube _cubemap : register(t0);
SamplerState _sample : register(s0);

cbuffer Cambuffer : register(b0)
{
    matrix mView;
    matrix mProjection;
    float4 mCamPos;
}

struct VS_SKYOUT
{ 
    float4 PosH : SV_POSITION;
    float3 Dir : Direction;
};

struct VS_SKYIN
{
    float3 Pos : POSITION;
};
