#include "Header.hlsli"

cbuffer cbQuadRenderer : register(b0)
{
    PostRenderer postRenderer;
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

float4 PS_RayMARCH(float4 position : SV_POSITION) : SV_Target
{
    // fxc /E PS_RayMARCH /T ps_5_0 ./PShaders.hlsl /Fo ./obj/PS_RayMARCH

    float4 posP;
    posP.x = +2.0f * position.x / postRenderer.rtSize.x - 1.0f;
    posP.y = -2.0f * position.y / postRenderer.rtSize.y + 1.0f;
    posP.z = 0.0f;
    posP.w = 1.0f;

    float3 rayOrigin = postRenderer.posCam;
    float3 rayDir = normalize(mul(posP, postRenderer.matPS2WS).xyz - rayOrigin);

    float3 sphereCenter1 = float3(-0.3f, 0.0f, 0.0f);
    float3 sphereCenter2 = float3(0.3f, 0.0f, 0.0f);
    float sphereRadius = 0.3f;

    float marchDistance = 0.0f;

    for (int i = 0; i < 20; i++)
    {
        float3 currentPos = rayOrigin + rayDir * marchDistance;
        float d1 = SphereSDF(currentPos, sphereCenter1, sphereRadius);
        float d2 = SphereSDF(currentPos, sphereCenter2, sphereRadius);
        float distance = SmoothMin(d1, d2, 10.0f);

        if (distance < 0.01f)
        {
            return float4(1.0f, 0.0f, 0.0f, 1.0f);
        }

        marchDistance += distance;
    }

    return float4(0.0f, 0.0f, 0.0f, 1.0f);
}