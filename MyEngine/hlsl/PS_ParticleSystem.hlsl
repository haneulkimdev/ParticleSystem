#include "Header.hlsli"

struct PixelIn
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
    float size : PARTICLESIZE;
    uint color : PARTICLECOLOR;
};

float4 main(PixelIn pin) : SV_TARGET
{
    return unpack_rgba(pin.color);
}