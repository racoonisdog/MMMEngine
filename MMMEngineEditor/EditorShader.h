#pragma once

namespace MMMEngine::EditorShader
{
    inline const char* g_shader_grid = R"(
// Vertex Shader Constant Buffer
cbuffer ViewProjBuffer : register(b0)
{
    float4x4 viewProj;
};

struct VS_INPUT { float3 position : POSITION; };
struct PS_INPUT { float4 position : SV_POSITION; float3 worldPosition : TEXCOORD0; };

PS_INPUT VS_Main(VS_INPUT input)
{
    PS_INPUT output;
    output.worldPosition = input.position;
    output.position = mul(float4(input.position, 1.0f), viewProj);
    return output;
}

// Pixel Shader Constant Buffer (C++의 PSSetConstantBuffers(0, ...)에 맞춤)
cbuffer CameraBuffer : register(b1)
{
    float3 cameraPosition;
    float cb_padding;
};

// 'line' 대신 'gridVal' 변수명 사용 (예약어 충돌 방지)
float drawGrid(float2 uv, float thickness)
{
    float2 derivative = fwidth(uv);
    derivative = max(derivative, float2(0.0001, 0.0001)); // 0 나누기 방지
    
    float2 gridPos = abs(frac(uv - 0.5) - 0.5) / derivative;
    float gridVal = min(gridPos.x, gridPos.y);
    
    return 1.0 - smoothstep(0.0, thickness, gridVal);
}

float axisLine(float distToAxis, float worldPerPixel, float pixelWidth, float maxWorldWidth)
{
    float w = min(pixelWidth * worldPerPixel, maxWorldWidth);
    return 1.0 - smoothstep(0.0, w, distToAxis); // 0~1
}

float4 PS_Main(PS_INPUT input) : SV_TARGET
{
    float2 uv = input.worldPosition.xz;
    float2 d  = max(fwidth(uv), float2(1e-4, 1e-4));

    float dist = length(input.worldPosition - cameraPosition);
    float fade = 1.0 - smoothstep(15.0, 45.0, dist);

    // --- grid 먼저 ---
    float g1 = drawGrid(uv, 1.0);
    float g2 = drawGrid(uv * 0.1, 1.0);
    float gridA = max(g1 * 0.15, g2 * 0.4) * fade;
    float3 gridRGB = float3(0.5, 0.5, 0.5);

    // --- axis 마스크 ---
    float pixelAxisWidth   = 1.0;
    float maxAxisWorldWide = 0.5; // 튜닝 포인트

    float zMask = axisLine(abs(input.worldPosition.x), d.x, pixelAxisWidth, maxAxisWorldWide);
    float xMask = axisLine(abs(input.worldPosition.z), d.y, pixelAxisWidth, maxAxisWorldWide);

    float3 zRGB = float3(0.2, 0.4, 1.0);
    float3 xRGB = float3(1.0, 0.3, 0.3);

    float axisA = max(zMask, xMask) * fade;

    // 축 색(원점은 섞이게)
    float3 axisRGB = (zRGB * zMask + xRGB * xMask);
    axisRGB = (axisRGB / max(zMask + xMask, 1e-4));

    // --- 합성: 그리드 위에 축 ---
    float3 outRGB = lerp(gridRGB, axisRGB, max(zMask, xMask));
    float  outA   = max(gridA, axisA);

    if (outA < 0.01) discard;
    return float4(outRGB, outA);
}
)";
}