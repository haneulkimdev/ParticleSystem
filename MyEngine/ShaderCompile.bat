if not exist "hlsl\objs\\" (
    mkdir "hlsl\objs"
)

fxc /E VSQuad /T vs_5_0 ./hlsl/VS_QUAD.hlsl /Fo ./hlsl/objs/VS_QUAD
fxc /E PS_RayMARCH /T ps_5_0 ./hlsl/PS_RayMARCH.hlsl /Fo ./hlsl/objs/PS_RayMARCH