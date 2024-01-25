#include "Header.hlsli"

struct VertexOut
{
    float4 pos : SV_POSITION;
    float size : PARTICLESIZE;
    uint color : PARTICLECOLOR;
};

StructuredBuffer<Particle> particles : register(t0);
StructuredBuffer<uint> aliveList : register(t1);
Buffer<float4> vertexBuffer_POSCOL : register(t2);
Buffer<float4> vertexBuffer_NOR : register(t3);

VertexOut main(uint vertexID : SV_VertexID)
{
    uint particleIndex = vertexID;
    
    // load particle data:
    Particle particle = particles[particleIndex];
    
    // calculate render properties from life:
    float lifeLerp = 1 - particle.life / particle.maxLife;
    float size = lerp(particle.sizeBeginEnd.x, particle.sizeBeginEnd.y, lifeLerp);
    
    VertexOut vout;
    vout.pos = float4(particle.position, 1);
    vout.size = size;
    vout.color = particle.color;
    return vout;
}