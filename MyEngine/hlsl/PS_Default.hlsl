cbuffer materialConstants : register(b10)
{
    float4 diffuseAlbedo;
    float3 fresnelR0;
    float roughness;
};

float4 main() : SV_TARGET
{
    return float4(0.8f, 0.8f, 0.8f, 1.0f);
}