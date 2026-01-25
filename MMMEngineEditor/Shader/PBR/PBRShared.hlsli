// PBR 20~39
TextureCube _specular : register(t20);
TextureCube _irradiance : register(t21);
Texture2D _brdflut : register(t22);

Texture2D _metalic : register(t30);
Texture2D _roughness : register(t31);
Texture2D _ambientOcclusion : register(t32);

// CB는 3부터 사용가능, 1:캠버퍼 1~2:라이트 버퍼

// PBR 메테리얼 버퍼
cbuffer MatBuffer : register(b3)
{
    float4 mBaseColor;
    
    float mMetalic;
    float mRoughness;
    float mAoStrength;
    float mEmissive;
}