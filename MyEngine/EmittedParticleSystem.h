#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>
#include <wrl/client.h>

#include <algorithm>
#include <memory>
#include <random>

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
};

static const int __CBUFFERBINDSLOT__EmittedParticleCB__ = 0;
struct alignas(16) EmittedParticleCB {
  float4x4 xEmitterTransform;
  float4x4 xEmitterBaseMeshUnormRemap;

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

struct IndirectDispatchArgs {
  uint32_t ThreadGroupCountX = 0;
  uint32_t ThreadGroupCountY = 0;
  uint32_t ThreadGroupCountZ = 0;
};

struct IndirectDrawArgsInstanced {
  uint32_t VertexCountPerInstance = 0;
  uint32_t InstanceCount = 0;
  uint32_t StartVertexLocation = 0;
  uint32_t StartInstanceLocation = 0;
};

struct Vertex {
  float3 position;
  uint color;
};

namespace my {
class MY_API EmittedParticleSystem {
 public:
  enum PARTICLESHADERTYPE {
    SIMPLE,
    PARTICLESHADERTYPE_COUNT,
  };

 private:
  ComPtr<ID3D11Buffer> particleBuffer;
  ComPtr<ID3D11Buffer> aliveList[2];
  ComPtr<ID3D11Buffer> deadList;
  ComPtr<ID3D11Buffer> counterBuffer;
  ComPtr<ID3D11Buffer> indirectBuffers;  // kickoffUpdate, simulation, draw
  ComPtr<ID3D11Buffer> constantBuffer;
  ComPtr<ID3D11Buffer> vertexBuffer;

  ComPtr<ID3D11ShaderResourceView> particleBufferSRV;
  ComPtr<ID3D11ShaderResourceView> aliveListSRV[2];
  ComPtr<ID3D11ShaderResourceView> deadListSRV;
  ComPtr<ID3D11ShaderResourceView> counterBufferSRV;
  ComPtr<ID3D11ShaderResourceView> constantBufferSRV;
  ComPtr<ID3D11ShaderResourceView> vertexBufferSRV;

  ComPtr<ID3D11UnorderedAccessView> particleBufferUAV;
  ComPtr<ID3D11UnorderedAccessView> aliveListUAV[2];
  ComPtr<ID3D11UnorderedAccessView> deadListUAV;
  ComPtr<ID3D11UnorderedAccessView> counterBufferUAV;
  ComPtr<ID3D11UnorderedAccessView> indirectBuffersUAV;
  ComPtr<ID3D11UnorderedAccessView> constantBufferUAV;
  ComPtr<ID3D11UnorderedAccessView> vertexBufferUAV;
  void CreateSelfBuffers();

  float emit = 0.0f;
  float burst = 0;
  float dt = 0;

  uint32_t MAX_PARTICLES = 1000;

 public:
  void UpdateCPU(const Matrix& transform, float dt);
  void Burst(int num);
  void Restart();

  void UpdateGPU(uint32_t instanceIndex);
  void Draw();

  enum FLAGS {
    FLAG_EMPTY = 0,
    FLAG_PAUSED = 1 << 0,
  };
  uint32_t _flags = FLAG_EMPTY;

  PARTICLESHADERTYPE shaderType = SIMPLE;

  float FIXED_TIMESTEP = -1.0f;  // -1 : variable timestep; >=0 : fixed timestep

  float size = 1.0f;
  float random_factor = 1.0f;
  float normal_factor = 1.0f;
  float count = 0.0f;
  float life = 1.0f;
  float random_life = 1.0f;
  float scaleX = 1.0f;
  float scaleY = 1.0f;
  float rotation = 0.0f;
  float motionBlurAmount = 0.0f;
  float mass = 1.0f;
  float random_color = 0;

  Vector3 velocity = {};  // starting velocity of all particles
  Vector3 gravity = {};   // constant gravity force
  float drag = 1.0f;  // constant drag (per frame velocity multiplier, reducing
                      // it will make particles slow down over time)
  float restitution =
      0.98f;  // if the particles have collision enabled, then after collision
              // this is a multiplier for their bouncing velocities

  // Sprite sheet properties:
  uint32_t framesX = 1;
  uint32_t framesY = 1;
  uint32_t frameCount = 1;
  uint32_t frameStart = 0;
  float frameRate = 0;  // frames per second

  void SetMaxParticleCount(uint32_t value);
  uint32_t GetMaxParticleCount() const { return MAX_PARTICLES; }
  uint64_t GetMemorySizeInBytes() const;

  Vector3 center;
  uint32_t layerMask = ~0u;
  Matrix worldMatrix = Matrix::Identity;

  inline bool IsPaused() const { return _flags & FLAG_PAUSED; }

  inline void SetPaused(bool value) {
    if (value) {
      _flags |= FLAG_PAUSED;
    } else {
      _flags &= ~FLAG_PAUSED;
    }
  }

  bool InitParticle(ComPtr<ID3D11Device>& device,
                    ComPtr<ID3D11DeviceContext>& context);

  void DeinitParticle();
};
}  // namespace my