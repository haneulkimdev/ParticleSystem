struct Particle
{
    float3 position;
    float mass;
    float3 force;
    float rotationalVelocity;
    float3 velocity;
    float maxLife;
    float2 sizeBeginEnd;
    float life;
    uint color;
};

struct ParticleCounters
{
    uint aliveCount;
    uint deadCount;
    uint realEmitCount;
    uint aliveCount_afterSimulation;
};
static const uint PARTICLECOUNTER_OFFSET_ALIVECOUNT = 0;
static const uint PARTICLECOUNTER_OFFSET_DEADCOUNT = PARTICLECOUNTER_OFFSET_ALIVECOUNT + 4;
static const uint PARTICLECOUNTER_OFFSET_REALEMITCOUNT = PARTICLECOUNTER_OFFSET_DEADCOUNT + 4;
static const uint PARTICLECOUNTER_OFFSET_ALIVECOUNT_AFTERSIMULATION = PARTICLECOUNTER_OFFSET_REALEMITCOUNT + 4;


cbuffer EmittedParticleCB : register(b0)
{
    float4x4 xEmitterTransform;
    float4x4 xEmitterBaseMeshUnormRemap;

    uint xEmitCount;
    float xEmitterRandomness;
    float xParticleRandomColorFactor;
    float xParticleSize;

    float xParticleScaling;
    float xParticleRotation;
    float xParticleRandomFactor;
    float xParticleNormalFactor;

    float xParticleLifeSpan;
    float xParticleLifeSpanRandomness;
    float xParticleMass;
    float xParticleMotionBlurAmount;

    uint xEmitterMaxParticleCount;
    uint xEmitterInstanceIndex;
    uint xEmitterMeshGeometryOffset;
    uint xEmitterMeshGeometryCount;

    uint xEmitterFramesX;
    uint xEmitterFramesY;
    uint xEmitterFrameCount;
    uint xEmitterFrameStart;

    float2 xEmitterTexMul;
    float xEmitterFrameRate;
    uint xEmitterLayerMask;

    float xSPH_h; // smoothing radius
    float xSPH_h_rcp; // 1.0f / smoothing radius
    float xSPH_h2; // smoothing radius ^ 2
    float xSPH_h3; // smoothing radius ^ 3

    float xSPH_poly6_constant; // precomputed Poly6 kernel constant term
    float xSPH_spiky_constant; // precomputed Spiky kernel function constant term
    float xSPH_visc_constant; // precomputed viscosity kernel function constant
                              // term
    float xSPH_K; // pressure constant

    float xSPH_e; // viscosity constant
    float xSPH_p0; // reference density
    uint xEmitterOptions;
    float xEmitterFixedTimestep; // we can force a fixed timestep (>0) onto the
                                // simulation to avoid blowing up

    float3 xParticleGravity;
    float xEmitterRestitution;

    float3 xParticleVelocity;
    float xParticleDrag;
};

static const uint THREADCOUNT_EMIT = 256;
static const uint THREADCOUNT_SIMULATION = 256;

static const uint ARGUMENTBUFFER_OFFSET_DISPATCHEMIT = 0;
static const uint ARGUMENTBUFFER_OFFSET_DISPATCHSIMULATION = ARGUMENTBUFFER_OFFSET_DISPATCHEMIT + (3 * 4);
static const uint ARGUMENTBUFFER_OFFSET_DRAWPARTICLES = ARGUMENTBUFFER_OFFSET_DISPATCHSIMULATION + (3 * 4);

struct Vertex
{
    float4 position : SV_POSITION;
    uint color : COLOR;
};

inline uint pack_rgba(in float4 value)
{
    uint retVal = 0;
    retVal |= (uint) (value.x * 255.0) << 0u;
    retVal |= (uint) (value.y * 255.0) << 8u;
    retVal |= (uint) (value.z * 255.0) << 16u;
    retVal |= (uint) (value.w * 255.0) << 24u;
    return retVal;
}
inline float4 unpack_rgba(in uint value)
{
    float4 retVal;
    retVal.x = (float) ((value >> 0u) & 0xFF) / 255.0;
    retVal.y = (float) ((value >> 8u) & 0xFF) / 255.0;
    retVal.z = (float) ((value >> 16u) & 0xFF) / 255.0;
    retVal.w = (float) ((value >> 24u) & 0xFF) / 255.0;
    return retVal;
}