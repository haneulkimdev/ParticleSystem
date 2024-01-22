#include "Header.hlsli"
#define EMIT_FROM_MESH
RWStructuredBuffer<Particle> particleBuffer : register(u0);
RWStructuredBuffer<uint> aliveBuffer_CURRENT : register(u1);
RWStructuredBuffer<uint> aliveBuffer_NEW : register(u2);
RWStructuredBuffer<uint> deadBuffer : register(u3);
RWByteAddressBuffer counterBuffer : register(u4);

#ifdef EMIT_FROM_MESH
ByteAddressBuffer meshVertexBuffer : register(t0);
ByteAddressBuffer meshIndexBuffer : register(t1);
#endif

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
    float4 baseColor = unpack_rgba(xParticleColor);

#ifdef EMIT_FROM_MESH
	// random triangle on emitter surface:
    const uint triangleCount = xEmitterMeshIndexCount / 3;
    const uint tri = rng.next_uint(triangleCount);

	// load indices of triangle from index buffer
    const uint emitterMeshIndexStride = 4;
    uint i0 = meshIndexBuffer.Load(tri * 3 * emitterMeshIndexStride);
    uint i1 = meshIndexBuffer.Load(tri * 3 * emitterMeshIndexStride + emitterMeshIndexStride);
    uint i2 = meshIndexBuffer.Load(tri * 3 * emitterMeshIndexStride + emitterMeshIndexStride * 2);

	// load vertices of triangle from vertex buffer:
    float3 pos0 = asfloat(meshVertexBuffer.Load3(i0 * xEmitterMeshVertexPositionStride));
    float3 pos1 = asfloat(meshVertexBuffer.Load3(i1 * xEmitterMeshVertexPositionStride));
    float3 pos2 = asfloat(meshVertexBuffer.Load3(i2 * xEmitterMeshVertexPositionStride));

	// random barycentric coords:
    float f = rng.next_float();
    float g = rng.next_float();
	[flatten]
    if (f + g > 1)
    {
        f = 1 - f;
        g = 1 - g;
    }
    float2 bary = float2(f, g);

	// compute final surface position on triangle from barycentric coords:
    emitPos = attribute_at_bary(pos0, pos1, pos2, bary);
    nor = normalize(cross(pos1 - pos0, pos2 - pos0));
    nor = normalize(mul(nor, (float3x3) xEmitterWorld));
    
#else
    // Just emit from center point:
    emitPos = 0;
#endif // EMIT_FROM_MESH
    
    float3 pos = mul(float4(emitPos, 1), xEmitterWorld).xyz;
    
    float particleStartingSize = xParticleSize + xParticleSize * (rng.next_float() - 0.5f) * xParticleRandomFactor;
    
    // create new particle:
    Particle particle = (Particle) 0;
    particle.position = pos;
    particle.force = 0;
    particle.mass = xParticleMass;
    particle.velocity = velocity + (nor + (float3(rng.next_float(), rng.next_float(), rng.next_float()) - 0.5f) * xParticleRandomFactor) * xParticleNormalFactor;
    particle.rotationalVelocity = xParticleRotation + (rng.next_float() - 0.5f) * xParticleRandomFactor;
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