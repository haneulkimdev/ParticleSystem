if not exist "hlsl\objs\\" (
    mkdir "hlsl\objs"
)

:: VS
fxc /E main /T vs_5_0 ./hlsl/VS_ParticleSystem.hlsl /Fo ./hlsl/objs/VS_ParticleSystem
fxc /E main /T vs_5_0 ./hlsl/VS_Particle.hlsl /Fo ./hlsl/objs/VS_Particle
fxc /E VSQuad /T vs_5_0 ./hlsl/VS_QUAD.hlsl /Fo ./hlsl/objs/VS_QUAD

:: PS
fxc /E main /T ps_5_0 ./hlsl/PS_ParticleSystem.hlsl /Fo ./hlsl/objs/PS_ParticleSystem
fxc /E main /T ps_5_0 ./hlsl/PS_Particle_Simple.hlsl /Fo ./hlsl/objs/PS_Particle_Simple
fxc /E PS_RayMARCH /T ps_5_0 ./hlsl/PS_RayMARCH.hlsl /Fo ./hlsl/objs/PS_RayMARCH

:: CS
fxc /E main /T cs_5_0 ./hlsl/CS_Particle_Emit.hlsl /Fo ./hlsl/objs/CS_Particle_Emit
fxc /E main /T cs_5_0 ./hlsl/CS_Particle_FinishUpdate.hlsl /Fo ./hlsl/objs/CS_Particle_FinishUpdate
fxc /E main /T cs_5_0 ./hlsl/CS_Particle_KickoffUpdate.hlsl /Fo ./hlsl/objs/CS_Particle_KickoffUpdate
fxc /E main /T cs_5_0 ./hlsl/CS_Particle_Simulate.hlsl /Fo ./hlsl/objs/CS_Particle_Simulate