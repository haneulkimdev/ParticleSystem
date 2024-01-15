#include "Header.hlsli"

struct VertexOut
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float life : PSIZE0;
    float size : PSIZE1;
};

StructuredBuffer<Particle> particles : register(t0);
StructuredBuffer<uint> aliveList : register(t1);

VertexOut main(uint vertexID : SV_VertexID)
{
    const float fadeLife = 0.2f;
    
    Particle particle = particles[vertexID];
    
    // calculate render properties from life:
    float lifeLerp = 1 - particle.life / particle.maxLife;
    float size = lerp(particle.sizeBeginEnd.x, particle.sizeBeginEnd.y, lifeLerp);
    
    VertexOut vout;
    
    vout.position = mul(float4(particle.position.xyz, 1.0), matWS2PS);
    vout.color = unpack_rgba(particle.color).xyz;
    vout.life = particle.life;
    vout.size = size;

    return vout;
}