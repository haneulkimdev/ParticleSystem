Texture2D g_color : register(t0);
Texture2D g_depth : register(t1);

struct PS_INPUT
{
    float4 position : SV_POSITION;
};

float4 PS_Light(PS_INPUT input) : SV_TARGET
{
    // fxc /E PS_Light /T ps_5_0 ./PS_Light.hlsl /Fo ./obj/PS_Light
    float4 color = g_color.Load(int3(input.position.xy, 0));
    
    return (color.a > 0.0f) ? float4(color.rgb, 1.0f)
                            : float4(0.0f, 0.0f, 0.0f, 1.0f);
}