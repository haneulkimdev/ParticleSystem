struct PS_INPUT
{
    float4 posH : SV_POSITION;
    float3 posW : POSITION;
    float4 color : COLOR;
    float3 normalW : NORMAL;
};

struct PS_OUTPUT
{
    float4 posColor : SV_Target0;
    float4 normal : SV_Target1;
};

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output;
    
    input.color *= 255.0f;

    uint color = (uint(input.color.a) << 24) | (uint(input.color.b) << 16) |
                 (uint(input.color.g) << 8) | uint(input.color.r);

    output.posColor = float4(input.posW, asfloat(color));
    
    output.normal = float4(input.normalW, 0.0f);

    return output;
}