#include "Header_Particle.hlsli"

ByteAddressBuffer counterBuffer : register(t0);

RWByteAddressBuffer indirectBuffers : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	uint particleCount = counterBuffer.Load(PARTICLECOUNTER_OFFSET_ALIVECOUNT);

    // Create draw argument buffer (VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation):
	indirectBuffers.Store4(ARGUMENTBUFFER_OFFSET_DRAWPARTICLES, uint4(4, particleCount, 0, 0));
}