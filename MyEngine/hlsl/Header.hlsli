#define MAX_PARTICLES 4
#define USE_SMOOTH_MIN_N

struct Particle
{
    float3 position;
    float size;
    uint color;
};

struct PostRenderer
{
    float3 posCam; // WS
    uint lightColor;

    float3 posLight; // WS
    float lightIntensity;

    float4x4 matPS2WS;

    float2 rtSize;
    float2 dummy0;

    float3 distBoxCenter; // WS
    float distBoxSize; // WS
};

struct Material
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Shininess;
};

float4 ColorConvertU32ToFloat4(uint color)
{
    float s = 1.0f / 255.0f;
    return float4(
        ((color) & 0xFF) * s,
        ((color >> 8) & 0xFF) * s,
        ((color >> 16) & 0xFF) * s,
        ((color >> 24) & 0xFF) * s);
}

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

float SphereSDF(float3 pos, float3 center, float radius)
{
    return length(pos - center) - radius;
}

float SmoothMax(float a, float b, float k)
{
    return log(exp(k * a) + exp(k * b)) / k;
}

float SmoothMin(float a, float b, float k)
{
    return -SmoothMax(-a, -b, k);
}

float SmoothMinN(float values[MAX_PARTICLES], int n, float k)
{
    float sum = 0.0f;
    for (int i = 0; i < n; i++)
    {
        sum += exp(-k * values[i]);
    }
    return -log(sum) / k;
}

float GetDist(float3 pos, StructuredBuffer<Particle> particles)
{
#ifndef USE_SMOOTH_MIN_N
    float distance = SphereSDF(pos, particles[0].position, particles[0].size / 2.0f);
    for (int i = 1; i < MAX_PARTICLES; i++)
    {
        distance = SmoothMin(distance, SphereSDF(pos, particles[i].position, particles[i].size / 2.0f), 10.0f);
    }
    return distance;
#else
    float distances[MAX_PARTICLES];
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        distances[i] = SphereSDF(pos, particles[i].position, particles[i].size / 2.0f);
    }
    return SmoothMinN(distances, MAX_PARTICLES, 10.0f);
#endif
}

float3 GetColor(float3 pos, StructuredBuffer<Particle> particles)
{
    float3 color = float3(0.0f, 0.0f, 0.0f);
    float weightSum = 0.0f;
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        float weight = 1.0f / SphereSDF(pos, particles[i].position, particles[i].size / 2.0f);
        color += ColorConvertU32ToFloat4(particles[i].color).rgb * weight;
        weightSum += weight;
    }
    color /= weightSum;
    return color;
}

float3 GetNormal(float3 pos, StructuredBuffer<Particle> particles)
{
    float distance = GetDist(pos, particles);
    float2 epsilon = float2(0.01f, 0.0f);
    float3 normal = distance - float3(
        GetDist(pos - epsilon.xyy, particles),
        GetDist(pos - epsilon.yxy, particles),
        GetDist(pos - epsilon.yyx, particles));
    return normalize(normal);
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