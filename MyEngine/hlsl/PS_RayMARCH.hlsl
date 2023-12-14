#include "Header.hlsli"

StructuredBuffer<Particle> particles : register(t0);

cbuffer cbQuadRenderer : register(b0)
{
    PostRenderer postRenderer;
}

float4 PS_RayMARCH(float4 position : SV_POSITION) : SV_Target
{
    // fxc /E PS_RayMARCH /T ps_5_0 ./PS_RayMARCH.hlsl /Fo ./obj/PS_RayMARCH
    float x = position.x;
    float y = position.y;

    float width = postRenderer.rtSize.x;
    float height = postRenderer.rtSize.y;
    
    float4 posP;
    posP.x = +2.0f * x / width - 1.0f;
    posP.y = -2.0f * y / height + 1.0f;
    posP.z = 0.0f;
    posP.w = 1.0f;

    float3 rayOrigin = postRenderer.posCam;
    float3 rayDir = normalize(mul(posP, postRenderer.matPS2WS).xyz - rayOrigin);

    float2 hits = ComputeAABBHits(
        rayOrigin, postRenderer.distBoxCenter - postRenderer.distBoxSize,
        postRenderer.distBoxCenter + postRenderer.distBoxSize, rayDir);
    
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
            float3 toEye = normalize(postRenderer.posCam - currentPos);

            float3 lightColor = ColorConvertU32ToFloat4(postRenderer.lightColor).rgb;

            float4 diffuseAlbedo = float4(GetColor(currentPos, particles), 1.0f);
            const float3 fresnelR0 = float3(0.05f, 0.05f, 0.05f);
            const float shininess = 0.8f;
            Material mat = { diffuseAlbedo, fresnelR0, shininess };

            float3 normal = GetNormal(currentPos, particles);
            
            float3 lightVec = normalize(postRenderer.posLight - currentPos);

            // Scale light down by Lambert's cosine law.
            float ndotl = max(dot(lightVec, normal), 0.0f);
            float3 lightStrength = lightColor * postRenderer.lightIntensity * ndotl;
            
            return float4(BlinnPhong(lightStrength, lightVec, normal, toEye, mat), 1.0f);
        }
        
        marchDistance += distance;
    }
    
    return float4(0.0f, 0.0f, 0.0f, 1.0f);
}