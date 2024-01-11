#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>
#include <wrl/client.h>

#include <fstream>
#include <random>
#include <vector>

#include "Camera.h"
#include "Helper.h"
#include "SimpleMath.h"
#include "spdlog/spdlog.h"

#define MY_API __declspec(dllexport)

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::SimpleMath;

using float4x4 = Matrix;
using float2 = Vector2;
using float3 = Vector3;
using uint = uint32_t;

struct Particle {
  float3 position;
  float3 velocity;
  float3 color;
  float life = 0.0f;
  float radius = 1.0f;
};

struct ParticleCounters {
  uint aliveCount;
  uint deadCount;
  uint realEmitCount;
};

struct ParticleSystemCB {
  uint xEmitCount;
  float xEmitterRandomness;
  float xParticleRandomColorFactor;
  float xParticleSize;

  float xParticleScaling;
  float xParticleRotation;
  float xParticleRandomFactor;
  float xParticleNormalFactor;

  float xParticleLifeSpan;
  float xParticleLifeSpanRandomness;
  float xParticleMass;
  float xParticleMotionBlurAmount;

  uint xEmitterMaxParticleCount;
  uint xEmitterInstanceIndex;
  uint xEmitterMeshGeometryOffset;
  uint xEmitterMeshGeometryCount;

  uint xEmitterFramesX;
  uint xEmitterFramesY;
  uint xEmitterFrameCount;
  uint xEmitterFrameStart;

  float2 xEmitterTexMul;
  float xEmitterFrameRate;
  uint xEmitterLayerMask;

  float xSPH_h;      // smoothing radius
  float xSPH_h_rcp;  // 1.0f / smoothing radius
  float xSPH_h2;     // smoothing radius ^ 2
  float xSPH_h3;     // smoothing radius ^ 3

  float xSPH_poly6_constant;  // precomputed Poly6 kernel constant term
  float xSPH_spiky_constant;  // precomputed Spiky kernel function constant term
  float xSPH_visc_constant;   // precomputed viscosity kernel function constant
                              // term
  float xSPH_K;               // pressure constant

  float xSPH_e;   // viscosity constant
  float xSPH_p0;  // reference density
  uint xEmitterOptions;
  float xEmitterFixedTimestep;  // we can force a fixed timestep (>0) onto the
                                // simulation to avoid blowing up

  float3 xParticleGravity;
  float xEmitterRestitution;

  float3 xParticleVelocity;
  float xParticleDrag;
};

struct PointLight {
  float3 position;
  float intensity;
  uint color;
};

struct FrameCB {
  float delta_time;
  uint frame_count;
  int dummy0;
  int dummy1;
};

struct PostRenderer {
  float3 posCam;  // WS
  uint lightColor;

  float3 posLight;  // WS
  float lightIntensity;

  float4x4 matWS2CS;
  float4x4 matPS2WS;

  float2 rtSize;
  float smoothingCoefficient;
  float deltaTime;

  float3 distBoxCenter;  // WS
  float distBoxSize;     // WS
};

namespace my {
extern "C" MY_API ParticleCounters GetStatistics();

extern "C" MY_API bool InitEngine(std::shared_ptr<spdlog::logger> spdlogPtr);

extern "C" MY_API bool SetRenderTargetSize(int w, int h);

extern "C" MY_API void Update(float dt);
extern "C" MY_API bool DoTest();
extern "C" MY_API bool GetDX11SharedRenderTarget(
    ID3D11Device* pDevice, ID3D11ShaderResourceView** ppSRView, int& w, int& h);

extern "C" MY_API void DeinitEngine();

extern "C" MY_API bool LoadShaders();

static void GPUBarrier();
}  // namespace my