#include "Header.hlsli"

struct VertexOut
{
    float4 pos : SV_POSITION;
    float size : PARTICLESIZE;
    uint color : PARTICLECOLOR;
};

StructuredBuffer<Particle> particles : register(t0);
StructuredBuffer<uint> aliveList : register(t1);
Buffer<float3> vertexBuffer_POS : register(t2);
Buffer<float3> vertexBuffer_NOR : register(t3);
Buffer<float4> vertexBuffer_COL : register(t4);

VertexOut main(uint vertexID : SV_VertexID)
{
    uint particleIndex = vertexID;
    
    float3 position = vertexBuffer_POS[particleIndex];
    float3 normal = vertexBuffer_NOR[particleIndex];
    float4 color = vertexBuffer_COL[particleIndex];
    
    // load particle data:
    Particle particle = particles[particleIndex];
    
    // calculate render properties from life:
    float lifeLerp = 1 - particle.life / particle.maxLife;
    float size = lerp(particle.sizeBeginEnd.x, particle.sizeBeginEnd.y, lifeLerp);
    
    float opacity = saturate(lerp(1, 0, lifeLerp));
    float4 particleColor = unpack_rgba(particle.color);
    particleColor.a *= opacity;
    
    VertexOut vout;
    //vout.pos = float4(position, 1);
    vout.pos = float4(particle.position, 1);
    vout.size = size;
    //vout.color = pack_rgba(color);
    vout.color = pack_rgba(particleColor);
    return vout;
}