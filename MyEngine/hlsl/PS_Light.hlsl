#include "Header.hlsli"

Texture2D g_color : register(t0);
Texture2D g_depth : register(t1);

cbuffer cbQuadRenderer : register(b0)
{
    PostRenderer postRenderer;
}

struct PS_INPUT
{
    float4 position : SV_POSITION;
};

float4 PS_Light(PS_INPUT input) : SV_TARGET
{
    // fxc /E PS_Light /T ps_5_0 ./PS_Light.hlsl /Fo ./obj/PS_Light
    float x = input.position.x;
    float y = input.position.y;
    
    float4 color = g_color.Load(int3(x, y, 0));

    if (color.a < 1.0f - 1e-5f)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    
    float width = postRenderer.rtSize.x;
    float height = postRenderer.rtSize.y;
    
    const float aspectRatio = width / height;
    
    float dzdx =
        (g_depth.Load(int3(x + 1, y, 0)).r - g_depth.Load(int3(x - 1, y, 0)).r) /
        (2.0f * (2.0f / width * aspectRatio));
    float dzdy =
        (g_depth.Load(int3(x, y + 1, 0)).r - g_depth.Load(int3(x, y - 1, 0)).r) /
        (2.0f * (2.0f / height));
    
    if (abs(dzdx) > 1e3f || abs(dzdy) > 1e3f)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
        
    float3 normal = normalize(float3(dzdx, -dzdy, -1.0f));
    
    float4 posP;
    posP.x = +2.0f * x / width - 1.0f;
    posP.y = -2.0f * y / height + 1.0f;
    posP.z = 0.0f;
    posP.w = 1.0f;

    float3 rayOrigin = postRenderer.posCam;
    float3 rayDir = normalize(mul(posP, postRenderer.matPS2WS).xyz - rayOrigin);
    
    float3 finalPos = rayOrigin + rayDir * g_depth.Load(int3(x, y, 0)).r;

    float3 toEye = normalize(postRenderer.posCam - finalPos);

    float3 lightColor =
        float3(float(postRenderer.lightColor & 0xFF) * (1.0f / 255.0f),
               float((postRenderer.lightColor >> 8) & 0xFF) * (1.0f / 255.0f),
               float((postRenderer.lightColor >> 16) & 0xFF) * (1.0f / 255.0f));

    const float3 fresnelR0 = float3(0.05f, 0.05f, 0.05f);
    const float shininess = 0.8f;
    Material mat = { color, fresnelR0, shininess };

    float3 lightVec = normalize(postRenderer.posLight - finalPos);

    // Scale light down by Lambert's cosine law.
    float ndotl = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = lightColor * postRenderer.lightIntensity * ndotl;

    return float4(BlinnPhong(lightStrength, lightVec, normal, toEye, mat), 1.0f);
}