#include "Header.hlsli"
#include "Header_Particle.hlsli"

static const float3 BILLBOARD[] =
{
	float3(-1, -1, 0), // 0
	float3(1, -1, 0), // 1
	float3(-1, 1, 0), // 2
	float3(1, 1, 0), // 4
};

cbuffer quadRendererCB : register(b1)
{
	PostRenderer postRenderer;
};

RWStructuredBuffer<Particle> particleBuffer : register(u0);
RWStructuredBuffer<uint> aliveBuffer_CURRENT : register(u1);
RWStructuredBuffer<uint> aliveBuffer_NEW : register(u2);
RWStructuredBuffer<uint> deadBuffer : register(u3);
RWByteAddressBuffer counterBuffer : register(u4);
RWStructuredBuffer<Vertex> vertexBuffer : register(u6);

[numthreads(THREADCOUNT_SIMULATION, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint Gid : SV_GroupIndex)
{
	uint aliveCount = counterBuffer.Load(PARTICLECOUNTER_OFFSET_ALIVECOUNT);
	if (DTid.x >= aliveCount)
		return;
		
	// simulation can be either fixed or variable timestep:
	const float dt = xEmitterFixedTimestep >= 0 ? xEmitterFixedTimestep : postRenderer.deltaTime;

	uint particleIndex = aliveBuffer_CURRENT[DTid.x];
	Particle particle = particleBuffer[particleIndex];
	uint v0 = particleIndex * 4;

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
		particle.life -= dt;

		// write back simulated particle:
		particleBuffer[particleIndex] = particle;

		// add to new alive list:
		uint newAliveIndex;
		counterBuffer.InterlockedAdd(PARTICLECOUNTER_OFFSET_ALIVECOUNT, 1, newAliveIndex);
		aliveBuffer_NEW[newAliveIndex] = particleIndex;

		// Write out render buffers:
		//	These must be persistent, not culled (raytracing, surfels...)

		float opacity = saturate(lerp(1, 0, lifeLerp));
		float4 particleColor = unpack_rgba(particle.color);
		particleColor.a *= opacity;

		float rotation = lifeLerp * particle.rotationalVelocity;
		float2x2 rot = float2x2(
			cos(rotation), -sin(rotation),
			sin(rotation), cos(rotation)
			);

		// Sprite sheet frame:
		const float spriteframe = xEmitterFrameRate == 0 ?
			lerp(xEmitterFrameStart, xEmitterFrameCount, lifeLerp) :
			((xEmitterFrameStart + particle.life * xEmitterFrameRate) % xEmitterFrameCount);
		const uint currentFrame = floor(spriteframe);
		const uint nextFrame = ceil(spriteframe);
		const float frameBlend = frac(spriteframe);
		uint2 offset = uint2(currentFrame % xEmitterFramesX, currentFrame / xEmitterFramesX);
		uint2 offset2 = uint2(nextFrame % xEmitterFramesX, nextFrame / xEmitterFramesX);

		for (uint vertexID = 0; vertexID < 4; ++vertexID)
		{
			// expand the point into a billboard in view space:
			float3 quadPos = BILLBOARD[vertexID];

			// rotate the billboard:
			quadPos.xy = mul(quadPos.xy, rot);

			// scale the billboard:
			quadPos *= particleSize;

			// scale the billboard along view space motion vector:
			float3 velocity = mul(particle.velocity, (float3x3) postRenderer.matWS2CS);
			quadPos += dot(quadPos, velocity) * velocity * xParticleMotionBlurAmount;

			// rotate the billboard to face the camera:
			quadPos = mul((float3x3) postRenderer.matWS2CS, quadPos); // reversed mul for inverse camera rotation!
            
			Vertex vertex;
			vertex.position = float4(particle.position + quadPos, 1);
			vertex.color = pack_rgba(particleColor);
            
			// write out vertex:
			vertexBuffer[v0 + vertexID] = vertex;
		}
	}
	else
	{
		// kill:
		uint deadIndex;
		counterBuffer.InterlockedAdd(PARTICLECOUNTER_OFFSET_DEADCOUNT, 1, deadIndex);
		deadBuffer[deadIndex] = particleIndex;
        
		Vertex vertex;
		vertex.position = 0;
		vertex.color = 0;
        
		vertexBuffer[v0 + 0] = vertex;
		vertexBuffer[v0 + 1] = vertex;
		vertexBuffer[v0 + 2] = vertex;
		vertexBuffer[v0 + 3] = vertex;
	}
}