#include "Header.hlsli"

struct VertexOut
{
    float4 pos : SV_POSITION;
    float size : PARTICLESIZE;
    uint color : PARTICLECOLOR;
};

struct GeoOut
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
    float size : PARTICLESIZE;
    uint color : PARTICLECOLOR;
};

static const float3 BILLBOARD[] =
{
    float3(-1, -1, 0),
	float3(-1, 1, 0),
	float3(1, -1, 0),
	float3(1, 1, 0),
};

StructuredBuffer<Particle> particles : register(t0);

[maxvertexcount(4)]
void main(
	point VertexOut gin[1], uint primID : SV_PrimitiveID,
	inout TriangleStream<GeoOut> triStream)
{
    // load particle data:
    Particle particle = particles[primID];
    if (particle.life <= 0)
        return;
    
    // calculate render properties from life:
    const float lifeLerp = 1 - particle.life / particle.maxLife;
    float rotation = lifeLerp * particle.rotationalVelocity;
    float2x2 rot = float2x2(cos(rotation), -sin(rotation), sin(rotation), cos(rotation));
        
    GeoOut gout;
    [unroll]
    for (uint i = 0; i < 4; i++)
    {
        // expand the point into a billboard in view space:
        float3 quadPos = BILLBOARD[i];
        float2 uv = quadPos.xy * float2(0.5f, -0.5f) + 0.5f;
        
        // rotate the billboard:
        quadPos.xy = mul(quadPos.xy, rot);
        
        // scale the billboard:
        quadPos.xy *= gin[0].size;
        
        // rotate the billboard to face the camera:
        quadPos = mul((float3x3) matWS2CS, quadPos); // reversed mul for inverse camera rotation!
        
        gout.pos = gin[0].pos + float4(quadPos, 0);
        gout.pos = mul(gout.pos, matWS2PS);
        gout.tex = uv;
        gout.size = gin[0].size;
        gout.color = gin[0].color;
        
        triStream.Append(gout);
    }
    
    triStream.RestartStrip();
}