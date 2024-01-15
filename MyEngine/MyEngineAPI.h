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
#include "CameraController.h"
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

struct ParticleEmitter {
  float size = 1.0f;
  float random_factor = 1.0f;
  float normal_factor = 1.0f;
  float count = 0.0f;
  float life = 1.0f;
  float random_life = 1.0f;
  float scale = 1.0f;
  float rotation = 0.0f;
  float mass = 1.0f;
  float random_color = 0;

  float velocity[3] = {};  // starting velocity of all new particles
  float gravity[3] = {};   // constant gravity force
  float drag = 1.0f;  // constant drag (per frame velocity multiplier, reducing
                      // it will make particles slow down over time)
  float restitution =
      0.98f;  // if the particles have collision enabled, then after collision
              // this is a multiplier for their bouncing velocities
};

struct ParticleSystemCB {
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
extern "C" MY_API ParticleEmitter* GetParticleEmitter();

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