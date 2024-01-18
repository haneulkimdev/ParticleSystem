#include "Header.hlsli"

RWStructuredBuffer<Particle> particleBuffer : register(u0);
RWStructuredBuffer<uint> aliveBuffer_CURRENT : register(u1);
RWStructuredBuffer<uint> aliveBuffer_NEW : register(u2);
RWStructuredBuffer<uint> deadBuffer : register(u3);
RWByteAddressBuffer counterBuffer : register(u4);

[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint aliveCount = counterBuffer.Load(PARTICLECOUNTER_OFFSET_ALIVECOUNT);
    if (DTid.x >= aliveCount)
        return;
    
    const float dt = delta_time;
    
    uint particleIndex = aliveBuffer_CURRENT[DTid.x];
    Particle particle = particleBuffer[particleIndex];
    
    const float lifeLerp = 1 - particle.life / particle.maxLife;
    const float particleSize = lerp(particle.sizeBeginEnd.x, particle.sizeBeginEnd.y, lifeLerp);
    
	// integrate:
    particle.force += particleGravity;
    particle.velocity += particle.force * dt;
    particle.position += particle.velocity * dt;
    
    // reset force for next frame:
    particle.force = 0;
    
    // drag: 
    particle.velocity *= particleDrag;
   
    if (particle.life > 0)
    {
        particle.life -= dt;
        
        float opacity = saturate(lerp(1, 0, lifeLerp));
        float4 particleColor = unpack_rgba(particle.color);
        particleColor.a *= opacity;
        
        particle.color = pack_rgba(particleColor);
        
        // write back simulated particle:
        particleBuffer[particleIndex] = particle;
        
        // add to new alive list:
        uint newAliveIndex;
        counterBuffer.InterlockedAdd(PARTICLECOUNTER_OFFSET_ALIVECOUNT_AFTERSIMULATION, 1, newAliveIndex);
        aliveBuffer_NEW[newAliveIndex] = particleIndex;
    }
    else
    {
        // kill:
        uint deadIndex;
        counterBuffer.InterlockedAdd(PARTICLECOUNTER_OFFSET_DEADCOUNT, 1, deadIndex);
        deadBuffer[deadIndex] = particleIndex;
    }
}