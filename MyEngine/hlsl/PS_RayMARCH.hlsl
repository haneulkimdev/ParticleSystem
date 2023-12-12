#include "Header.hlsli"

StructuredBuffer<Particle> particles : register(t0);

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
    output.depth = -1e3f;
    
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

    float marchDistance = 0.0f;
    
    float distances[MAX_PARTICLES];
    float3 colors[MAX_PARTICLES];
    
    [loop]
    for (int i = 0; i < 100; i++)
    {
        float3 currentPos = rayOrigin + rayDir * marchDistance;
        
        float weightSum = 0.0f;
        float3 color = float3(0.0f, 0.0f, 0.0f);
        
        for (int j = 0; j < MAX_PARTICLES; j++)
        {
            distances[j] = SphereSDF(currentPos, particles[j].position, particles[j].size / 2.0f);
            colors[j] = float3(float(particles[j].color & 0xFF) * (1.0f / 255.0f),
                               float((particles[j].color >> 8) & 0xFF) * (1.0f / 255.0f),
                               float((particles[j].color >> 16) & 0xFF) * (1.0f / 255.0f));
            
            float weight = 1.0f / distances[j];
            weightSum += weight;
            color += colors[j] * weight;
        }
        
        color /= max(weightSum, 1e-5f);
        
        float distance = SmoothMinN(distances, MAX_PARTICLES, 10.0f);
        
        if (distance < 1e-5f)
        {
            output.color = float4(color, 1.0f);
            output.depth = marchDistance;
            return output;
        }

        marchDistance += distance;
    }

    return output;
}