struct VertexOut
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float life : PSIZE0;
    float size : PSIZE1;
};

struct Particle
{
    float3 position;
    float3 velocity;
    float3 color;
    float life;
    float size;
};

StructuredBuffer<Particle> particles : register(t0);
StructuredBuffer<uint> aliveList : register(t1);

VertexOut main(uint vertexID : SV_VertexID)
{
    const float fadeLife = 0.2f;
    
    Particle p = particles[vertexID];
    
    VertexOut vout;
    
    vout.position = float4(p.position.xyz, 1.0);
    vout.color = p.color * saturate(p.life / fadeLife);
    vout.life = p.life;
    vout.size = p.size;

    return vout;
}