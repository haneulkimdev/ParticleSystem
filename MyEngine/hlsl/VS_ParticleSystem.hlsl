#include "Header.hlsli"

struct VertexOut
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float life : PSIZE0;
    float size : PSIZE1;
};

StructuredBuffer<Particle> particles : register(t0);
StructuredBuffer<uint> aliveList : register(t1);
Buffer<float4> vertexBuffer_POSCOL : register(t2);
Buffer<float4> vertexBuffer_NOR : register(t3);

VertexOut main(uint vertexID : SV_VertexID)
{
    Particle particle = particles[vertexID];
    
    // calculate render properties from life:
    float lifeLerp = 1 - particle.life / particle.maxLife;
    float size = lerp(particle.sizeBeginEnd.x, particle.sizeBeginEnd.y, lifeLerp);
    
    VertexOut vout;
    
    vout.position = float4(particle.position, 1.0f);
    vout.position = mul(vout.position, matWS2PS);
    vout.color = unpack_rgba(particle.color);
    vout.life = particle.life;
    vout.size = size;

    return vout;
}