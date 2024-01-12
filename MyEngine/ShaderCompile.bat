if not exist "hlsl\objs\\" (
    mkdir "hlsl\objs"
)

:: VS
fxc /E main /T vs_5_0 ./hlsl/VS_ParticleSystem.hlsl /Fo ./hlsl/objs/VS_ParticleSystem

:: GS
fxc /E main /T gs_5_0 ./hlsl/GS_ParticleSystem.hlsl /Fo ./hlsl/objs/GS_ParticleSystem

:: PS
fxc /E main /T ps_5_0 ./hlsl/PS_ParticleSystem.hlsl /Fo ./hlsl/objs/PS_ParticleSystem

:: CS
fxc /E main /T cs_5_0 ./hlsl/CS_ParticleSystem_KickoffUpdate.hlsl /Fo ./hlsl/objs/CS_ParticleSystem_KickoffUpdate
fxc /E main /T cs_5_0 ./hlsl/CS_ParticleSystem_Emit.hlsl /Fo ./hlsl/objs/CS_ParticleSystem_Emit
fxc /E main /T cs_5_0 ./hlsl/CS_ParticleSystem_Simulate.hlsl /Fo ./hlsl/objs/CS_ParticleSystem_Simulate