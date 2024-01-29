#include "Header.hlsli"

cbuffer objectConstants : register(b10)
{
    float4x4 world;
};

float4 main(float4 pos : POSITION) : SV_POSITION
{
    return mul(mul(pos, world), matWS2PS);
}