#include "Header.hlsli"

RWStructuredBuffer<Particle> particleBuffer : register(u0);
RWStructuredBuffer<uint> aliveBuffer_CURRENT : register(u1);
RWStructuredBuffer<uint> aliveBuffer_NEW : register(u2);
RWStructuredBuffer<uint> deadBuffer : register(u3);
RWByteAddressBuffer counterBuffer : register(u4);

[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint emitCount = counterBuffer.Load(PARTICLECOUNTER_OFFSET_REALEMITCOUNT);
    if (DTid.x >= emitCount)
        return;
  
    RNG rng;
    rng.init(uint2(xEmitterRandomness, DTid.x), frame_count);
    
    float3 emitPos = 0;
    float3 nor = 0;
    float3 velocity = xParticleVelocity;
    float4 baseColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
    
    float particleStartingSize = xParticleSize + xParticleSize * (rng.next_float() - 0.5f) * xParticleRandomFactor;
    
    // create new particle:
    Particle particle = (Particle) 0;
    particle.position = emitPos;
    particle.force = 0;
    particle.mass = xParticleMass;
    particle.velocity = velocity + (nor + (float3(rng.next_float(), rng.next_float(), rng.next_float()) - 0.5f) * xParticleRandomFactor) * xParticleNormalFactor;
    particle.rotationalVelocity = xParticleRotation * PI * 60 + (rng.next_float() - 0.5f) * xParticleRandomFactor;
    particle.maxLife = xParticleLifeSpan + xParticleLifeSpan * (rng.next_float() - 0.5f) * xParticleLifeSpanRandomness;
    particle.life = particle.maxLife;
    particle.sizeBeginEnd = float2(particleStartingSize, particleStartingSize * xParticleScaling);
    
    baseColor.r *= lerp(1, rng.next_float(), xParticleRandomColorFactor);
    baseColor.g *= lerp(1, rng.next_float(), xParticleRandomColorFactor);
    baseColor.b *= lerp(1, rng.next_float(), xParticleRandomColorFactor);
    particle.color = pack_rgba(baseColor);
    
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