#include "Header.hlsli"

RWStructuredBuffer<Particle> particleBuffer : register(u0);
RWStructuredBuffer<uint> aliveBuffer_CURRENT : register(u1);
RWStructuredBuffer<uint> aliveBuffer_NEW : register(u2);
RWStructuredBuffer<uint> deadBuffer : register(u3);
RWByteAddressBuffer counterBuffer : register(u4);
RWByteAddressBuffer vertexBuffer_POS : register(u5);
RWByteAddressBuffer vertexBuffer_NOR : register(u6);
RWBuffer<float4> vertexBuffer_COL : register(u7);

static const uint VERTEXBUFFER_POS_STRIDE = 12;

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
    particle.force += xParticleGravity;
    particle.velocity += particle.force * dt;
    particle.position += particle.velocity * dt;
    
    // reset force for next frame:
    particle.force = 0;
    
    // drag: 
    particle.velocity *= xParticleDrag;
   
    if (particle.life > 0)
    {
        // floor collision:
        if (particle.position.y - particleSize < floorHeight)
        {
            particle.position.y = particleSize + floorHeight;
            particle.velocity.y *= -xEmitterRestitution;
        }
        
        particle.life -= dt;
        
        // write back simulated particle:
        particleBuffer[particleIndex] = particle;
        
        // add to new alive list:
        uint newAliveIndex;
        counterBuffer.InterlockedAdd(PARTICLECOUNTER_OFFSET_ALIVECOUNT_AFTERSIMULATION, 1, newAliveIndex);
        aliveBuffer_NEW[newAliveIndex] = particleIndex;
        
        // Write out render buffers:
        float opacity = saturate(lerp(1, 0, lifeLerp));
        float4 particleColor = unpack_rgba(particle.color);
        particleColor.a *= opacity;
        
        vertexBuffer_POS.Store3(particleIndex * VERTEXBUFFER_POS_STRIDE, particle.position);
        vertexBuffer_NOR.Store3(particleIndex * VERTEXBUFFER_POS_STRIDE, normalize(-camDir));
        vertexBuffer_COL[particleIndex] = particleColor;
    }
    else
    {
        // kill:
        uint deadIndex;
        counterBuffer.InterlockedAdd(PARTICLECOUNTER_OFFSET_DEADCOUNT, 1, deadIndex);
        deadBuffer[deadIndex] = particleIndex;
        
        vertexBuffer_POS.Store3(particleIndex * VERTEXBUFFER_POS_STRIDE, 0);
    }
}