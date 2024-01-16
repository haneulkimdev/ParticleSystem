#include "Header.hlsli"

struct VertexOut
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float life : PSIZE0;
    float size : PSIZE1;
};

struct GeoOut
{
    float4 pos : SV_POSITION; // not POSITION
    float2 texCoord : TEXCOORD;
    float4 color : COLOR;
    uint primID : SV_PrimitiveID;
};

StructuredBuffer<Particle> particles : register(t0);

[maxvertexcount(4)]
void main(
	point VertexOut gin[1], uint primID : SV_PrimitiveID,
	inout TriangleStream<GeoOut> triStream)
{
    if (gin[0].life < 0.0f)
        return;
    
    Particle particle = particles[primID];
    
    const float lifeLerp = 1 - particle.life / particle.maxLife;
    const float particleSize = lerp(particle.sizeBeginEnd.x, particle.sizeBeginEnd.y, lifeLerp);
    
    float rotation = lifeLerp * particle.rotationalVelocity;
    float2x2 rot = float2x2(cos(rotation), -sin(rotation), sin(rotation), cos(rotation));
    
    float hw = particleSize / 2.0f;
    float3 up = float3(0.0f, 1.0f, 0.0f);
    float3 right = float3(1.0f, 0.0f, 0.0f);
    
    up.xy = mul(up.xy, rot);
    right.xy = mul(right.xy, rot);
    
    GeoOut gout;
    gout.pos.w = 1;
    gout.color = gin[0].color;
    
    gout.pos.xyz = gin[0].position.xyz - hw * right - hw * up;
    gout.texCoord = float2(0.0, 1.0);
    gout.primID = primID;
    triStream.Append(gout);
    
    gout.pos.xyz = gin[0].position.xyz - hw * right + hw * up;
    gout.texCoord = float2(0.0, 0.0);
    gout.primID = primID;
    triStream.Append(gout);
    
    gout.pos.xyz = gin[0].position.xyz + hw * right - hw * up;
    gout.texCoord = float2(1.0, 1.0);
    gout.primID = primID;
    triStream.Append(gout);
    
    gout.pos.xyz = gin[0].position.xyz + hw * right + hw * up;
    gout.texCoord = float2(1.0, 0.0);
    gout.primID = primID;
    triStream.Append(gout);
    
    triStream.RestartStrip();
}