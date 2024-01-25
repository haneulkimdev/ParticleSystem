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

static const float3 BILLBOARD[] =
{
    float3(-1, -1, 0), // 0
	float3(-1, 1, 0), // 2
	float3(1, -1, 0), // 1
	float3(1, 1, 0), // 3
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
        
    GeoOut gout;
    gout.pos.w = 1;
    gout.color = gin[0].color;
    
    for (uint i = 0; i < 4; i++)
    {
        // expand the point into a billboard in view space:
        float3 quadPos = BILLBOARD[i];
        float2 uv = quadPos.xy * float2(0.5f, -0.5f) + 0.5f;
        
        // rotate the billboard:
        quadPos.xy = mul(quadPos.xy, rot);
        
        // scale the billboard:
        quadPos.xy *= particleSize;
        
        // rotate the billboard to face the camera:
        quadPos = mul((float3x3) matWS2CS, quadPos); // reversed mul for inverse camera rotation!
        
        gout.pos.xyz = gin[0].position.xyz + quadPos;
        gout.texCoord = uv;
        gout.primID = primID;
        
        triStream.Append(gout);
    }
    
    triStream.RestartStrip();
}