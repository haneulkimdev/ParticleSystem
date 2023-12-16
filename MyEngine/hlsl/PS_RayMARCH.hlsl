#include "Header.hlsli"

#define MAX_STEPS 100
#define SURF_DIST 1e-5f
#define SURF_REFINEMENT 5

StructuredBuffer<Particle> particleBuffer : register(t0);

cbuffer cbQuadRenderer : register(b0)
{
    PostRenderer quadRenderer;
}

float SphereSDF(float3 pos, float3 center, float radius)
{
    return length(pos - center) - radius;
}

float SmoothMax(float a, float b, float k)
{
    return log(exp(k * a) + exp(k * b)) / k;
}

float SmoothMin(float a, float b, float k)
{
    return -SmoothMax(-a, -b, k);
}

float SmoothMinN(float values[MAX_PARTICLES], int n, float k)
{
    float sum = 0.0f;
    for (int i = 0; i < n; i++)
    {
        sum += exp(-k * values[i]);
    }
    return -log(sum) / k;
}

float GetLifeLerp(int index)
{
    return 1 - particleBuffer[index].life / particleBuffer[index].maxLife;
}

float GetParticleSize(int index)
{
    float lifeLerp = GetLifeLerp(index);
    return lerp(particleBuffer[index].sizeBeginEnd.x, particleBuffer[index].sizeBeginEnd.y, lifeLerp);
}

float GetDist(float3 pos)
{
#define USE_SMOOTH_MIN_N
#ifdef USE_SMOOTH_MIN_N
    float distances[MAX_PARTICLES];
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        float3 center = particleBuffer[i].position;
        float radius = GetParticleSize(i) / 2.0f;
        distances[i] = SphereSDF(pos, center, radius);
    }
    float k = quadRenderer.smoothingCoefficient;
    return SmoothMinN(distances, MAX_PARTICLES, k);
#else
    float3 center = particleBuffer[0].position;
    float radius = GetParticleSize(0) / 2.0f;
    float distance = SphereSDF(pos, center, radius);
    for (int i = 1; i < MAX_PARTICLES; i++)
    {
        center = particleBuffer[i].position;
        radius = GetParticleSize(i) / 2.0f;
        float k = quadRenderer.smoothingCoefficient;
        distance = SmoothMin(distance, SphereSDF(pos, center, radius), k);
    }
    return distance;
#endif
}

float3 GetNormal(float3 pos)
{
    float distance = GetDist(pos);
    float2 epsilon = float2(0.01f, 0.0f);
    float3 normal = distance - float3(
        GetDist(pos - epsilon.xyy),
        GetDist(pos - epsilon.yxy),
        GetDist(pos - epsilon.yyx));
    return normalize(normal);
}

float3 GetColor(float3 pos)
{
    float3 color = float3(0.0f, 0.0f, 0.0f);
    float weightSum = 0.0f;
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        float3 center = particleBuffer[i].position;
        float radius = GetParticleSize(i) / 2.0f;
        float weight = 1.0f / SphereSDF(pos, center, radius);
        color += ColorConvertU32ToFloat4(particleBuffer[i].color).rgb * weight;
        weightSum += weight;
    }
    color /= weightSum;
    return color;
}

float3 GetLight(float3 pos)
{
    float3 toEye = normalize(quadRenderer.posCam - pos);

    float3 lightColor = ColorConvertU32ToFloat4(quadRenderer.lightColor).rgb;

    float4 diffuseAlbedo = float4(GetColor(pos), 1.0f);
    const float3 fresnelR0 = float3(0.05f, 0.05f, 0.05f);
    const float shininess = 0.8f;
    Material mat = { diffuseAlbedo, fresnelR0, shininess };

    float3 normal = GetNormal(pos);
            
    float3 lightVec = normalize(quadRenderer.posLight - pos);

    // Scale light down by Lambert's cosine law.
    float ndotl = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = lightColor * quadRenderer.lightIntensity * ndotl;
            
    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
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
        
        float distance = GetDist(currentPos);
        
        if (distance <= 0.0f)
#ifdef SURF_REFINEMENT
        {
            float3 st = currentPos - rayDir * SURF_DIST;
            float3 en = currentPos;
            
            [loop]
            for (int j = 0; j < SURF_REFINEMENT; j++)
            {
                float3 mid = (st + en) / 2.0f;
                if (GetDist(mid) > 0.0f)
                {
                    st = mid;
                }
                else
                {
                    en = mid;
                }
                currentPos = mid;
            }
            
            return float4(GetLight(currentPos), 1.0f);
        }
        
        marchDistance += max(distance, SURF_DIST);
#else
        {
            return float4(GetLight(currentPos), 1.0f);
        }
        
        marchDistance += distance;
#endif
    }
    return float4(0.0f, 0.0f, 0.0f, 1.0f);
}