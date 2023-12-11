struct PostRenderer
{
    float3 posCam; // WS
    int lightColor;

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