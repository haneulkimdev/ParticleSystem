struct PixelIn
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float life : PSIZE0;
    float size : PSIZE1;
};

float4 main(PixelIn pin) : SV_TARGET
{
    if (pin.life < 0.0f)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    
    return float4(pin.color, 1.0f);
}