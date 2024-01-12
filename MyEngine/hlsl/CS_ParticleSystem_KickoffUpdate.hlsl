#include "Header.hlsli"

RWByteAddressBuffer counterBuffer : register(u4);

[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // Load dead particle count:
    uint deadCount = counterBuffer.Load(PARTICLECOUNTER_OFFSET_DEADCOUNT);

	// Load alive particle count:
    uint aliveCount_NEW = counterBuffer.Load(PARTICLECOUNTER_OFFSET_ALIVECOUNT_AFTERSIMULATION);

	// we can not emit more than there are free slots in the dead list:
    uint realEmitCount = min(deadCount, emitCount);

	// copy new alivelistcount to current alivelistcount:
    counterBuffer.Store(PARTICLECOUNTER_OFFSET_ALIVECOUNT, aliveCount_NEW);

	// reset new alivecount:
    counterBuffer.Store(PARTICLECOUNTER_OFFSET_ALIVECOUNT_AFTERSIMULATION, 0);

	// write real emit count:
    counterBuffer.Store(PARTICLECOUNTER_OFFSET_REALEMITCOUNT, realEmitCount);
}