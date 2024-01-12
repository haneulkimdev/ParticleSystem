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
  float mass;
  float3 force;
  float rotationalVelocity;
  float3 velocity;
  float maxLife;
  float2 sizeBeginEnd;
  float life;
  uint color;
};

struct ParticleCounters {
  uint aliveCount;
  uint deadCount;
  uint realEmitCount;
  uint aliveCount_afterSimulation;
};

struct EmitterProperties {
  uint emitCount;
  float emitterRandomness;
  float particleRandomColorFactor;
  float particleSize;

  float particleScaling;
  float particleRotation;
  float particleRandomFactor;
  float particleNormalFactor;

  float particleLifeSpan;
  float particleLifeSpanRandomness;
  float particleMass;
  uint emitterMaxParticleCount;

  float3 particleGravity;
  float emitterRestitution;

  float3 particleVelocity;
  float particleDrag;
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
extern "C" MY_API EmitterProperties* GetEmitterProperties();

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