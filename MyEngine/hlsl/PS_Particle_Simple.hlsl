#include "Header_Particle.hlsli"

float4 main(Vertex input) : SV_TARGET
{
    // fxc /E main /T ps_5_0 ./PS_Particle_Simple.hlsl /Fo ./obj/PS_Particle_Simple
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}