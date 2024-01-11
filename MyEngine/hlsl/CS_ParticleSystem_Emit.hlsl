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
    const uint emitCount = 10;
    
	// we can not emit more than there are free slots in the dead list:
    uint realEmitCount = min(counterBuffer.Load(PARTICLECOUNTER_OFFSET_DEADCOUNT), emitCount);
    
    // write real emit count:
    counterBuffer.Store(PARTICLECOUNTER_OFFSET_REALEMITCOUNT, realEmitCount);
    if (DTid.x >= realEmitCount)
        return;
    
    float3 rainbow[7] =
    {
        float3(1.0f, 0.0f, 0.0f), // Red
        float3(1.0f, 0.65f, 0.0f), // Orange
        float3(1.0f, 1.0f, 0.0f), // Yellow
        float3(0.0f, 1.0f, 0.0f), // Green
        float3(0.0f, 0.0f, 1.0f), // Blue
        float3(0.3f, 0.0f, 0.5f), // Indigo
        float3(0.5f, 0.0f, 1.0f) // Violet/Purple
    };
    
    RNG rng;
    rng.init(DTid.xx, frameCB.frame_count);
    
    float theta = rng.next_float() * 2.0f * 3.141592f;
    float speed = rng.next_float() * 0.5f + 1.5f;
    float life = rng.next_float();
    
    // create new particle:
    Particle particle;
    particle.position = float3(cos(theta), -sin(theta), 0.0f) * life * 0.1f + float3(0.0f, 0.3f, 0.0f);
    particle.velocity = float3(-1.0f, 0.0f, 0.0f) * speed;
    particle.color = rainbow[rng.next_uint(6)];
    particle.life = life * 1.5f;
    particle.radius = 1.0f;
    
    // new particle index retrieved from dead list (pop):
    uint deadCount;
    counterBuffer.InterlockedAdd(PARTICLECOUNTER_OFFSET_DEADCOUNT, -1, deadCount);
    uint newParticleIndex = deadBuffer[deadCount - 1];
    
    // write out the new particle:
    particleBuffer[newParticleIndex] = particle;
    
    // and add index to the alive list (push):
    uint aliveCount;
    counterBuffer.InterlockedAdd(PARTICLECOUNTER_OFFSET_ALIVECOUNT, 1, aliveCount);
    aliveBuffer_CURRENT[aliveCount] = newParticleIndex;
}