struct PixelIn
{
    float4 pos : SV_POSITION; // not POSITION
    float2 texCoord : TEXCOORD;
    float4 color : COLOR;
    uint primID : SV_PrimitiveID;
};

float4 main(PixelIn pin) : SV_TARGET
{
    return float4(pin.color.rgb, 0.5);
}