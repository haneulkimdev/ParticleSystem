cbuffer cbPerObject : register(b0)
{
    float4x4 g_world;
    float4x4 g_view;
    float4x4 g_projection;
};

struct VS_INPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
    float3 normal : NORMAL;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    
    float4 position = float4(input.position, 1.0f);
	
	// Transform to homogeneous clip space.
    position = mul(position, g_world);
    position = mul(position, g_view);
    position = mul(position, g_projection);
    output.position = position;
	
	// Just pass vertex color into the pixel shader.
    output.color = input.color;
    
    return output;
}