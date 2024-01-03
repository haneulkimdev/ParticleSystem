#include "Header_Particle.hlsli"

RWByteAddressBuffer counterBuffer : register(u4);
RWByteAddressBuffer indirectBuffers : register(u5);

[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // fxc /E main /T cs_5_0 ./CS_Particle_KickoffUpdate.hlsl /Fo ./obj/CS_Particle_KickoffUpdate
    
	// Load dead particle count:
    uint deadCount = counterBuffer.Load(PARTICLECOUNTER_OFFSET_DEADCOUNT);

	// Load alive particle count:
    uint aliveCount = counterBuffer.Load(PARTICLECOUNTER_OFFSET_ALIVECOUNT);

	// we can not emit more than there are free slots in the dead list:
    uint realEmitCount = min(deadCount, xEmitCount);

	// Fill dispatch argument buffer for emitting (ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ):
    indirectBuffers.Store3(ARGUMENTBUFFER_OFFSET_DISPATCHEMIT, uint3(ceil((float) realEmitCount / (float) THREADCOUNT_EMIT), 1, 1));

	// Fill dispatch argument buffer for simulation (ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ):
    indirectBuffers.Store3(ARGUMENTBUFFER_OFFSET_DISPATCHSIMULATION, uint3(ceil((float) (aliveCount + realEmitCount) / (float) THREADCOUNT_SIMULATION), 1, 1));

	// reset new alivecount:
    counterBuffer.Store(PARTICLECOUNTER_OFFSET_ALIVECOUNT, 0);

	// and write real emit count:
    counterBuffer.Store(PARTICLECOUNTER_OFFSET_REALEMITCOUNT, realEmitCount);
}