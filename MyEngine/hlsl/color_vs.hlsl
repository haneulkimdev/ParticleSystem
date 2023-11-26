cbuffer cbPerObject : register(b0)
{
    float4x4 g_world;
    float4x4 g_worldInvTranspose;
    float4x4 g_view;
    float4x4 g_proj;
};

struct VS_INPUT
{
    float3 posL : POSITION;
    float4 color : COLOR;
    float3 normalL : NORMAL;
    float2 texC : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 posH : SV_POSITION;
    float4 color : COLOR;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    
    float4 pos = float4(input.posL, 1.0f);
	
	// Transform to homogeneous clip space.
    pos = mul(pos, g_world);
    pos = mul(pos, g_view);
    pos = mul(pos, g_proj);
    output.posH = pos;
	
	// Just pass vertex color into the pixel shader.
    output.color = input.color;
    
    return output;
}