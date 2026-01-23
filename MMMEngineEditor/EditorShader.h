#pragma once

namespace MMMEngine::EditorShader
{
	inline const char* g_shader_grid = R"(
// Vertex Shader
cbuffer ViewProjBuffer : register(b0)
{
    float4x4 viewProj;
};

struct VS_INPUT
{
    float3 position : POSITION;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
};

PS_INPUT VS_Main(VS_INPUT input)
{
    PS_INPUT output;
    output.worldPosition = input.position;
    output.position = mul(float4(input.position, 1.0f), viewProj);
    return output;
}

// Pixel Shader
cbuffer CameraBuffer : register(b0) 
{
    float3 cameraPosition;
    float padding;
};
float4 PS_Main(PS_INPUT input) : SV_TARGET
{
    float2 uv = input.worldPosition.xz / 1.0; // gridSpacing = 1.0
    float2 uvDeriv = fwidth(uv);
    
    // 선 두께 계산 (GLSL 로직 그대로)
    float2 scaledThickness = uvDeriv * 1.0; // lineThickness = 1.0
    
    // frac은 GLSL의 fract와 동일
    float distX = abs(frac(uv.x + scaledThickness.x / 2.0));
    float distZ = abs(frac(uv.y + scaledThickness.y / 2.0));

    bool onXLine = distX < scaledThickness.x;
    bool onZLine = distZ < scaledThickness.y;
    bool onAxisX = abs(input.worldPosition.z) < scaledThickness.y / 2.0;
    bool onAxisZ = abs(input.worldPosition.x) < scaledThickness.x / 2.0;

    float distFromCamera = length(input.worldPosition.xz - cameraPosition.xz);
    float alpha = 1.0 - smoothstep(4.0, 24.0, distFromCamera); // fadeDistance, fadeRange 적용

    if (onAxisZ) return float4(0.0, 0.0, 1.0, alpha);      // axisZColor
    if (onAxisX) return float4(1.0, 0.0, 0.0, alpha);      // axisXColor
    if (onXLine || onZLine) return float4(0.5, 0.5, 0.5, alpha); // gridColor
    
    discard;
    return float4(0,0,0,0);
}
)";
}




