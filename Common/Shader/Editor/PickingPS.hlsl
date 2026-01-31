cbuffer PickingBuffer : register(b5)
{
    uint g_ObjectId;
    uint3 g_Padding;
}

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
};

uint main(PS_INPUT input) : SV_Target0
{
    return g_ObjectId;
}
