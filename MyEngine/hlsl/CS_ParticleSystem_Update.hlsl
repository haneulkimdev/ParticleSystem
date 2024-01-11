#include "Header.hlsli"

cbuffer cbFrame : register(b0)
{
    FrameCB frameCB;
}

cbuffer cbParticleSystem : register(b1)
{
    ParticleSystemCB particleSystemCB;
}

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
    
    const float dt = frameCB.delta_time * 0.5;
    
    uint particleIndex = aliveBuffer_CURRENT[DTid.x];
    Particle particle = particleBuffer[particleIndex];
    
    const float3 gravity = float3(0.0f, -9.8f, 0.0f);
    
    particle.velocity += gravity * dt;
    particle.position += particle.velocity * dt;
   
    if (particle.life > 0)
    {
        particle.life -= dt;
        
        // write back simulated particle:
        particleBuffer[particleIndex] = particle;
    }
    else
    {
        uint aliveIndex;
        counterBuffer.InterlockedAdd(PARTICLECOUNTER_OFFSET_ALIVECOUNT, -1, aliveIndex);
        
        // kill:
        uint deadIndex;
        counterBuffer.InterlockedAdd(PARTICLECOUNTER_OFFSET_DEADCOUNT, 1, deadIndex);
        deadBuffer[deadIndex] = particleIndex;
    }
}