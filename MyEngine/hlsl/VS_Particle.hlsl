#include "Header.hlsli"
#include "Header_Particle.hlsli"

static const float3 BILLBOARD[] =
{
    float3(-1, -1, 0), // 0
	float3(1, -1, 0), // 1
	float3(-1, 1, 0), // 2
	float3(1, 1, 0), // 4
};

StructuredBuffer<Particle> particleBuffer : register(t0);
StructuredBuffer<uint> aliveList : register(t1);
StructuredBuffer<Vertex> vertexBuffer : register(t2);

struct VS_PARTICLE_OUTPUT
{
    float4 pos : SV_POSITION;
    nointerpolation float size : SIZE;
    nointerpolation uint color : COLOR;
};

VS_PARTICLE_OUTPUT main(uint vid : SV_VertexID, uint instanceID : SV_InstanceID)
{
    uint particleIndex = aliveList[instanceID];
    uint vertexID = particleIndex * 4 + vid;

    float3 position = vertexBuffer[vertexID].position.xyz;
    float4 color = vertexBuffer[vertexID].color;

	// load particle data:
    Particle particle = particleBuffer[particleIndex];

	// calculate render properties from life:
    float lifeLerp = 1 - particle.life / particle.maxLife;
    float size = lerp(particle.sizeBeginEnd.x, particle.sizeBeginEnd.y, lifeLerp);

    VS_PARTICLE_OUTPUT output;
    output.pos = float4(position, 1.0f);
    output.size = size;
    output.color = pack_rgba(color);
    return output;
}