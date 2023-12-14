#include "Header.hlsli"

#define MAX_STEPS 100
#define SURF_DIST 1e-5f
#define SURF_REFINEMENT 5

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
    for (int i = 0; i < MAX_STEPS; i++)
    {
        float3 currentPos = rayOrigin + rayDir * marchDistance;
        
        float distance = GetDist(currentPos, particles);
        
        if (distance <= 0.0f)
#ifdef SURF_REFINEMENT
        {
            float3 st = currentPos - rayDir * SURF_DIST;
            float3 en = currentPos;
            
            [loop]
            for (int j = 0; j < SURF_REFINEMENT; j++)
            {
                float3 mid = (st + en) / 2.0f;
                if (GetDist(mid, particles) > 0.0f)
                {
                    st = mid;
                }
                else
                {
                    en = mid;
                }
                currentPos = mid;
            }
            
            return float4(GetLight(currentPos, particles, quadRenderer), 1.0f);
        }
        
        marchDistance += max(distance, SURF_DIST);
#else
        {
            return float4(GetLight(currentPos, particles, quadRenderer), 1.0f);
        }
        
        marchDistance += distance;
#endif
    }
    return float4(0.0f, 0.0f, 0.0f, 1.0f);
}