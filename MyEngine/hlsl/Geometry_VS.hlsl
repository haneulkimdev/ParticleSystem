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
    float3 posW : POSITION;
    float4 color : COLOR;
    float3 normalW : NORMAL;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    
    // Transform to world space.
    float4 posW = mul(float4(input.posL, 1.0f), g_world);
    output.posW = posW.xyz;
    
    output.normalW = mul(input.normalL, (float3x3) g_worldInvTranspose);
    
	// Transform to homogeneous clip space.
    float4 posV = mul(posW, g_view);
    output.posH = mul(posV, g_proj);
	
	// Just pass vertex color into the pixel shader.
    output.color = input.color;
    
    return output;
}