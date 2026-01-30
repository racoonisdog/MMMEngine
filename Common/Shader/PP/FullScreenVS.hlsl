// 풀스크린 트라이앵글 VS
struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

VSOutput main(uint vertexID : SV_VertexID)
{
    VSOutput output;

    // 정점 ID에 따라 삼각형 좌표 지정
    float2 pos;
    if (vertexID == 0) pos = float2(-1.0, -1.0); // 왼쪽 아래
    if (vertexID == 1) pos = float2(-1.0,  3.0); // 왼쪽 위 (화면을 넘겨서 삼각형 확장)
    if (vertexID == 2) pos = float2( 3.0, -1.0); // 오른쪽 아래 (화면을 넘겨서 삼각형 확장)

    output.position = float4(pos, 0.0, 1.0);

    // UV 좌표 계산 (0~1 범위)
    output.uv = (pos * 0.5f) + 0.5f;
    output.uv.g *= -1.0f;

    return output;
}