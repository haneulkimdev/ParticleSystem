#include "Header_Particle.hlsli"

struct PS_PARTICLE_INPUT
{
    float4 pos : SV_POSITION;
    nointerpolation float size : SIZE;
    nointerpolation uint color : COLOR;
};

float4 main(PS_PARTICLE_INPUT input) : SV_TARGET
{
	return float4(1.0f, 1.0f, 1.0f, 1.0f);
}