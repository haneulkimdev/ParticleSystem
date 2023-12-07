#include "Header.hlsli"
    
cbuffer cbQuadRenderer : register(b0)
{
    PostRenderer postRenderer;
}

float4 PS_RayMARCH(float4 position : SV_POSITION) : SV_Target
{
    // fxc /E PS_RayMARCH /T ps_5_0 ./PShaders.hlsl /Fo ./obj/PS_RayMARCH
    return float4(1, 0, 0, 1);
}