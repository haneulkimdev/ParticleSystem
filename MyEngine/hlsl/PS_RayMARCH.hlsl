#include "Header.hlsli"

cbuffer cbQuadRenderer : register(b0)
{
    PostRenderer postRenderer;
}

struct PS_INPUT
{
    float4 position : SV_POSITION;
};

struct PS_OUTPUT
{
    float4 color : SV_Target0;
    float depth : SV_Target1;
};

PS_OUTPUT PS_RayMARCH(PS_INPUT input)
{
    // fxc /E PS_RayMARCH /T ps_5_0 ./PS_RayMARCH.hlsl /Fo ./obj/PS_RayMARCH
    PS_OUTPUT output;
    output.color = float4(0.0f, 0.0f, 0.0f, 0.0f);
    output.depth = -10000.0f;
    
    float4 posP;
    posP.x = +2.0f * input.position.x / postRenderer.rtSize.x - 1.0f;
    posP.y = -2.0f * input.position.y / postRenderer.rtSize.y + 1.0f;
    posP.z = 0.0f;
    posP.w = 1.0f;

    float3 rayOrigin = postRenderer.posCam;
    float3 rayDir = normalize(mul(posP, postRenderer.matPS2WS).xyz - rayOrigin);

    float2 hits = ComputeAABBHits(
        rayOrigin, postRenderer.distBoxCenter - postRenderer.distBoxSize,
        postRenderer.distBoxCenter + postRenderer.distBoxSize, rayDir);
    
    if (hits.y < 0.0f)
    {
        return output;
    }

    if (hits.x > hits.y)
    {
        return output;
    }

    float3 sphereCenter1 = float3(-0.3f, 0.0f, 0.0f);
    float3 sphereCenter2 = float3(0.3f, 0.0f, 0.0f);
    float sphereRadius = 0.3f;
    float3 sphereColor1 = float3(1.0f, 0.0f, 0.0f);
    float3 sphereColor2 = float3(0.0f, 0.0f, 1.0f);

    float marchDistance = 0.0f;

    for (int i = 0; i < 20; i++)
    {
        float3 currentPos = rayOrigin + rayDir * marchDistance;
        float d1 = SphereSDF(currentPos, sphereCenter1, sphereRadius);
        float d2 = SphereSDF(currentPos, sphereCenter2, sphereRadius);
        float distance = SmoothMin(d1, d2, 10.0f);
        float3 color = lerp(sphereColor1, sphereColor2, saturate(d1 / (d1 + d2)));

        if (distance < 0.01f)
        {
            output.color = float4(color, 1.0f);
            output.depth = marchDistance;
            return output;
        }

        marchDistance += distance;
    }

    return output;
}