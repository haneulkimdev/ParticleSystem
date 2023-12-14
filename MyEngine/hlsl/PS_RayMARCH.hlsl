#include "Header.hlsli"

StructuredBuffer<Particle> particles : register(t0);

cbuffer cbQuadRenderer : register(b0)
{
    PostRenderer quadRenderer;
}

float4 PS_RayMARCH(float4 position : SV_POSITION) : SV_Target
{
    // fxc /E PS_RayMARCH /T ps_5_0 ./PS_RayMARCH.hlsl /Fo ./obj/PS_RayMARCH
    float x = position.x;
    float y = position.y;

    float width = quadRenderer.rtSize.x;
    float height = quadRenderer.rtSize.y;
    
    float4 posP;
    posP.x = +2.0f * x / width - 1.0f;
    posP.y = -2.0f * y / height + 1.0f;
    posP.z = 0.0f;
    posP.w = 1.0f;

    float3 rayOrigin = quadRenderer.posCam;
    float3 rayDir = normalize(mul(posP, quadRenderer.matPS2WS).xyz - rayOrigin);

    float2 hits = ComputeAABBHits(
        rayOrigin, quadRenderer.distBoxCenter - quadRenderer.distBoxSize,
        quadRenderer.distBoxCenter + quadRenderer.distBoxSize, rayDir);
    
    if (hits.x > hits.y)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    float marchDistance = hits.x;
    
    [loop]
    for (int i = 0; i < 100; i++)
    {
        float3 currentPos = rayOrigin + rayDir * marchDistance;
        
        float distance = GetDist(currentPos, particles);
        
        if (distance < 1e-5f)
        {
            return float4(GetLight(currentPos, particles, quadRenderer), 1.0f);
        }
        
        marchDistance += distance;
    }
    
    return float4(0.0f, 0.0f, 0.0f, 1.0f);
}