Texture2D g_MaskTex : register(t0);

cbuffer OutlineBuffer : register(b0)
{
    float4 g_OutlineColor;
    float2 g_TexelSize;
    float2 g_Padding0;
    float g_Thickness;
    float g_Threshold;
    float2 g_Padding1;
}

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

float SampleMaskCoord(int2 coord)
{
    int2 maxCoord = int2(1.0 / g_TexelSize) - 1;
    coord = clamp(coord, int2(0, 0), maxCoord);
    return g_MaskTex.Load(int3(coord, 0)).r;
}

float4 main(VSOutput input) : SV_Target
{
    int t = (int)max(1.0, g_Thickness);
    int2 base = int2(input.position.xy);

    float grid[9];
    grid[0] = SampleMaskCoord(base + int2(-t,  t));
    grid[1] = SampleMaskCoord(base + int2( 0,  t));
    grid[2] = SampleMaskCoord(base + int2( t,  t));
    grid[3] = SampleMaskCoord(base + int2(-t,  0));
    grid[4] = SampleMaskCoord(base);
    grid[5] = SampleMaskCoord(base + int2( t,  0));
    grid[6] = SampleMaskCoord(base + int2(-t, -t));
    grid[7] = SampleMaskCoord(base + int2( 0, -t));
    grid[8] = SampleMaskCoord(base + int2( t, -t));

    float sx = (grid[2] + 2.0 * grid[5] + grid[8]) - (grid[0] + 2.0 * grid[3] + grid[6]);
    float sy = (grid[0] + 2.0 * grid[1] + grid[2]) - (grid[6] + 2.0 * grid[7] + grid[8]);

    float dist = sqrt(sx * sx + sy * sy);
    float edge = saturate((dist - g_Threshold) * 4.0f);

    return float4(g_OutlineColor.rgb, g_OutlineColor.a * edge);
}
