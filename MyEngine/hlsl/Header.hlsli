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