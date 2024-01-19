static const float PI = 3.14159265358979323846;

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
};
static const uint PARTICLECOUNTER_OFFSET_ALIVECOUNT = 0;
static const uint PARTICLECOUNTER_OFFSET_DEADCOUNT = PARTICLECOUNTER_OFFSET_ALIVECOUNT + 4;
static const uint PARTICLECOUNTER_OFFSET_REALEMITCOUNT = PARTICLECOUNTER_OFFSET_DEADCOUNT + 4;
static const uint PARTICLECOUNTER_OFFSET_ALIVECOUNT_AFTERSIMULATION = PARTICLECOUNTER_OFFSET_REALEMITCOUNT + 4;

cbuffer cbFrame : register(b0)
{
    uint frame_count;
    float time;
    float time_previous;
    float delta_time;
};

cbuffer cbParticleSystem : register(b1)
{
    float4x4 xEmitterWorld;

    uint xEmitCount;
    uint xEmitterMeshIndexCount;
    uint xEmitterMeshVertexPositionStride;
    float xEmitterRandomness;

    float xParticleSize;
    float xParticleScaling;
    float xParticleRotation;
    uint xParticleColor;

    float xParticleRandomFactor;
    float xParticleNormalFactor;
    float xParticleLifeSpan;
    float xParticleLifeSpanRandomness;

    float xParticleMass;
    float xParticleMotionBlurAmount;
    float xParticleRandomColorFactor;
    uint xEmitterMaxParticleCount;

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
    float xSPH_visc_constant; // precomputed viscosity kernel function constant term
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

cbuffer cbQuadRenderer : register(b2)
{
    float3 posCam; // WS
    uint lightColor;

    float3 posLight; // WS
    float lightIntensity;

    float4x4 matWS2PS;
    float4x4 matPS2WS;

    float2 rtSize;
    float smoothingCoefficient;
    float floorHeight;

    float3 distBoxCenter; // WS
    float distBoxSize; // WS
};

struct Material
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Shininess;
};

float2 ComputeAABBHits(const float3 posStart, const float3 posMin,
                       const float3 posMax, const float3 vecDir)
{
    // intersect ray with a box
    // https://education.siggraph.org/static/HyperGraph/raytrace/rtinter3.htm
    float3 invR = float3(1.0f, 1.0f, 1.0f) / vecDir;
    float3 tbot = invR * (posMin - posStart);
    float3 ttop = invR * (posMax - posStart);

     // re-order intersections to find smallest and largest on each axis
    float3 tmin = min(ttop, tbot);
    float3 tmax = max(ttop, tbot);

     // find the largest tmin and the smallest tmax
    float largestTmin = max(max(tmin.x, tmin.y), max(tmin.x, tmin.z));
    float smallestTmax = min(min(tmax.x, tmax.y), min(tmax.x, tmax.z));

    float tnear = max(largestTmin, 0.f);
    float tfar = smallestTmax;
    return float2(tnear, tfar);
}

// Schlick gives an approximation to Fresnel reflectance (see pg. 233 "Real-Time Rendering 3rd Ed.").
// R0 = ( (n-1)/(n+1) )^2, where n is the index of refraction.
float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
{
    float cosIncidentAngle = saturate(dot(normal, lightVec));

    float f0 = 1.0f - cosIncidentAngle;
    float3 reflectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);

    return reflectPercent;
}

float3 BlinnPhong(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
    const float m = mat.Shininess * 256.0f;
    float3 halfVec = normalize(toEye + lightVec);

    float roughnessFactor = (m + 8.0f) * pow(max(dot(halfVec, normal), 0.0f), m) / 8.0f;
    float3 fresnelFactor = SchlickFresnel(mat.FresnelR0, halfVec, lightVec);

    float3 specAlbedo = fresnelFactor * roughnessFactor;

    // Our spec formula goes outside [0,1] range, but we are 
    // doing LDR rendering.  So scale it down a bit.
    specAlbedo = specAlbedo / (specAlbedo + 1.0f);

    return (mat.DiffuseAlbedo.rgb + specAlbedo) * lightStrength;
}

// Random number generator based on: https://github.com/diharaw/helios/blob/master/src/engine/shader/random.glsl
struct RNG
{
    uint2 s; // state

	// xoroshiro64* random number generator.
	// http://prng.di.unimi.it/xoroshiro64star.c
    uint rotl(uint x, uint k)
    {
        return (x << k) | (x >> (32 - k));
    }
	// Xoroshiro64* RNG
    uint next()
    {
        uint result = s.x * 0x9e3779bb;

        s.y ^= s.x;
        s.x = rotl(s.x, 26) ^ s.y ^ (s.y << 9);
        s.y = rotl(s.y, 13);

        return result;
    }
	// Thomas Wang 32-bit hash.
	// http://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11/
    uint hash(uint seed)
    {
        seed = (seed ^ 61) ^ (seed >> 16);
        seed *= 9;
        seed = seed ^ (seed >> 4);
        seed *= 0x27d4eb2d;
        seed = seed ^ (seed >> 15);
        return seed;
    }

    void init(uint2 id, uint frameIndex)
    {
        uint s0 = (id.x << 16) | id.y;
        uint s1 = frameIndex;
        s.x = hash(s0);
        s.y = hash(s1);
        next();
    }
    float next_float()
    {
        uint u = 0x3f800000 | (next() >> 9);
        return asfloat(u) - 1.0;
    }
    uint next_uint(uint nmax)
    {
        float f = next_float();
        return uint(floor(f * nmax));
    }
    float2 next_float2()
    {
        return float2(next_float(), next_float());
    }
    float3 next_float3()
    {
        return float3(next_float(), next_float(), next_float());
    }
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