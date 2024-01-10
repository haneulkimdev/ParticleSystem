if not exist "hlsl\objs\\" (
    mkdir "hlsl\objs"
)

:: VS
fxc /E main /T vs_5_0 ./hlsl/VS_ParticleSystem.hlsl /Fo ./hlsl/objs/VS_ParticleSystem
fxc /E VSQuad /T vs_5_0 ./hlsl/VS_QUAD.hlsl /Fo ./hlsl/objs/VS_QUAD

:: PS
fxc /E main /T ps_5_0 ./hlsl/PS_ParticleSystem.hlsl /Fo ./hlsl/objs/PS_ParticleSystem
fxc /E PS_RayMARCH /T ps_5_0 ./hlsl/PS_RayMARCH.hlsl /Fo ./hlsl/objs/PS_RayMARCH

:: CS
fxc /E main /T cs_5_0 ./hlsl/CS_ParticleSystem_Emit.hlsl /Fo ./hlsl/objs/CS_ParticleSystem_Emit
fxc /E main /T cs_5_0 ./hlsl/CS_ParticleSystem_Update.hlsl /Fo ./hlsl/objs/CS_ParticleSystem_Update