#include "Header_Particle.hlsli"

RWStructuredBuffer<Particle> particleBuffer : register(u0);
RWStructuredBuffer<uint> aliveBuffer_CURRENT : register(u1);
RWStructuredBuffer<uint> aliveBuffer_NEW : register(u2);
RWStructuredBuffer<uint> deadBuffer : register(u3);
RWByteAddressBuffer counterBuffer : register(u4);

[numthreads(THREADCOUNT_EMIT, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // fxc /E main /T cs_5_0 ./CS_Particle_Emit.hlsl /Fo ./obj/CS_Particle_Emit
    uint emitCount = counterBuffer.Load(PARTICLECOUNTER_OFFSET_REALEMITCOUNT);
    if (DTid.x >= emitCount)
        return;

    const float4x4 worldMatrix = xEmitterTransform;
    float3 emitPos = 0;
    float3 nor = 0;
    float3 velocity = xParticleVelocity;
    float4 baseColor = float4(1, 1, 1, 1);

    // Just emit from center point:
    emitPos = 0;
    
    float3 pos = mul(float4(emitPos, 1), worldMatrix).xyz;
    
    float particleStartingSize = xParticleSize;

    // create new particle:
    Particle particle;
    particle.position = pos;
    particle.force = 0;
    particle.mass = xParticleMass;
    particle.velocity = velocity;
    particle.rotationalVelocity = xParticleRotation;
    particle.maxLife = xParticleLifeSpan;
    particle.life = particle.maxLife;
    particle.sizeBeginEnd = float2(particleStartingSize, particleStartingSize * xParticleScaling);
    particle.color = pack_rgba(baseColor);
    
    // new particle index retrived from dead list (pop):
    uint deadCount;
    counterBuffer.InterlockedAdd(PARTICLECOUNTER_OFFSET_DEADCOUNT, -1, deadCount);
    uint newParticleIndex = deadBuffer[deadCount - 1];

    
    // write out the new particle:
    particleBuffer[newParticleIndex] = particle;
    
    // and add index to the alive list (push):
    uint aliveCount;
    counterBuffer.InterlockedAdd(PARTICLECOUNTER_OFFSET_ALIVECOUNT, 1, aliveCount);
    aliveBuffer_NEW[aliveCount] = newParticleIndex;
}