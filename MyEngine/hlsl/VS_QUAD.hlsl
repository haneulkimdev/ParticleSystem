struct VS_QUAD_INPUT
{
    float3 position : POSITION;
};

struct VS_QUAD_OUTPUT
{
    float4 position : SV_POSITION;
};

VS_QUAD_OUTPUT VSQuad(VS_QUAD_INPUT input)
{
    VS_QUAD_OUTPUT output;
    output.position = float4(input.position, 1);
    return output;
}