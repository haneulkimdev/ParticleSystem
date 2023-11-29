struct VS_INPUT
{
    float3 posL : POSITION;
    float4 color : COLOR;
    float3 normalL : NORMAL;
    float2 texC : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 posL : SV_POSITION;
    float2 texC : TEXCOORD;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    
    output.posL = float4(input.posL, 1.0f);
    
    output.texC = input.texC;
    
    return output;
}