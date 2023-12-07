#include "Header.hlsli"
    
cbuffer cbQuadRenderer : register(b0)
{
    PostRenderer postRenderer;
}

float4 PS_RayMARCH(float4 position : SV_POSITION) : SV_Target
{
    // fxc /E PS_RayMARCH /T ps_5_0 ./PShaders.hlsl /Fo ./obj/PS_RayMARCH
    float3 p0 = float3(-0.3f, 0.0f, 0.0f);
    float3 p1 = float3(0.3f, 0.0f, 0.0f);
    float r0 = 0.3f;
    float r1 = 0.3f;

    float4 posP;
    posP.x = +2.0f * position.x / postRenderer.rtSize.x - 1.0f;
    posP.y = -2.0f * position.y / postRenderer.rtSize.y + 1.0f;
    posP.z = 0.0f;
    posP.w = 1.0f;

    float3 posW = mul(posP, postRenderer.matPS2WS).xyz;

    float x = posW.x;
    float y = posW.y;
    float z = posW.z;

    float f0 = (x - p0.x) * (x - p0.x) + (y - p0.y) * (y - p0.y) +
               (z - p0.z) * (z - p0.z) - r0 * r0;
    float f1 = (x - p1.x) * (x - p1.x) + (y - p1.y) * (y - p1.y) +
               (z - p1.z) * (z - p1.z) - r1 * r1;

    return (f0 < 0.0f || f1 < 0.0f) ? float4(1.0f, 0.0f, 0.0f, 1.0f)
                                    : float4(0.0f, 0.0f, 0.0f, 1.0f);
}