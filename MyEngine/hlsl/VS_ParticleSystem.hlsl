#include "Header.hlsli"

struct VertexOut
{
    float4 pos : SV_POSITION;
    float size : PARTICLESIZE;
    uint color : PARTICLECOLOR;
};

StructuredBuffer<Particle> particles : register(t0);
StructuredBuffer<uint> aliveList : register(t1);

VertexOut main(uint vertexID : SV_VertexID)
{
    uint particleIndex = vertexID;
    
    // load particle data:
    Particle particle = particles[particleIndex];
    
    // calculate render properties from life:
    float lifeLerp = 1 - particle.life / particle.maxLife;
    float size = lerp(particle.sizeBeginEnd.x, particle.sizeBeginEnd.y, lifeLerp);
    
    float opacity = saturate(lerp(1, 0, lifeLerp));
    float4 particleColor = unpack_rgba(particle.color);
    particleColor.a *= opacity;
    
    VertexOut vout;
    vout.pos = float4(particle.position, 1);
    vout.size = size;
    vout.color = pack_rgba(particleColor);
    return vout;
}