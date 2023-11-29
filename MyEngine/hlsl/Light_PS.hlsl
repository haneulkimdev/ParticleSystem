Texture2D g_posColor : register(t0);
Texture2D g_normal : register(t1);

struct PS_INPUT
{
    float4 posL : SV_POSITION;
    float2 texC : TEXCOORD;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    uint width;
    uint height;
    g_posColor.GetDimensions(width, height);
    
    uint color = asuint(
        g_posColor.Load(int3(input.texC.x * width, input.texC.y * height, 0)).a);

    return float4(float((color >> 16) & 0xFF) * (1.0f / 255.0f),
                  float((color >> 8) & 0xFF) * (1.0f / 255.0f),
                  float(color & 0xFF) * (1.0f / 255.0f),
                  float((color >> 24) & 0xFF) * (1.0f / 255.0f));
}