#include "EmittedParticleSystem.h"

static const uint ARGUMENTBUFFER_OFFSET_DISPATCHEMIT = 0;
static const uint ARGUMENTBUFFER_OFFSET_DISPATCHSIMULATION =
    ARGUMENTBUFFER_OFFSET_DISPATCHEMIT + (3 * 4);
static const uint ARGUMENTBUFFER_OFFSET_DRAWPARTICLES =
    ARGUMENTBUFFER_OFFSET_DISPATCHSIMULATION + (3 * 4);

namespace my {
static ComPtr<ID3D11Device> device;
static ComPtr<ID3D11DeviceContext> context;

static ComPtr<ID3D11VertexShader> vertexShader;
static ComPtr<ID3D11PixelShader>
    pixelShader[EmittedParticleSystem::PARTICLESHADERTYPE_COUNT];
static ComPtr<ID3D11ComputeShader> kickoffUpdateCS;
static ComPtr<ID3D11ComputeShader> finishUpdateCS;
static ComPtr<ID3D11ComputeShader> emitCS;
static ComPtr<ID3D11ComputeShader> simulateCS;

static ComPtr<ID3D11RasterizerState> rasterizerState;
static ComPtr<ID3D11DepthStencilState> depthStencilState;

void EmittedParticleSystem::SetMaxParticleCount(uint32_t value) {
  MAX_PARTICLES = value;
  counterBuffer.Reset();  // will be recreated
}

void EmittedParticleSystem::CreateSelfBuffers() {
  D3D11_BUFFER_DESC particleBufferDesc = {};
  if (particleBuffer) particleBuffer->GetDesc(&particleBufferDesc);

  if (particleBufferDesc.ByteWidth < MAX_PARTICLES * sizeof(Particle)) {
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    D3D11_SUBRESOURCE_DATA data = {};

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;

    // Particle buffer:
    bd.StructureByteStride = sizeof(Particle);
    bd.ByteWidth = bd.StructureByteStride * MAX_PARTICLES;
    device->CreateBuffer(&bd, nullptr, particleBuffer.ReleaseAndGetAddressOf());

    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.BufferEx.FirstElement = 0;
    srvDesc.BufferEx.NumElements = MAX_PARTICLES;
    device->CreateShaderResourceView(
        particleBuffer.Get(), &srvDesc,
        particleBufferSRV.ReleaseAndGetAddressOf());

    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = MAX_PARTICLES;
    device->CreateUnorderedAccessView(
        particleBuffer.Get(), &uavDesc,
        particleBufferUAV.ReleaseAndGetAddressOf());

    // Alive index lists (double buffered):
    bd.StructureByteStride = sizeof(uint32_t);
    bd.ByteWidth = bd.StructureByteStride * MAX_PARTICLES;
    device->CreateBuffer(&bd, nullptr, aliveList[0].ReleaseAndGetAddressOf());
    device->CreateBuffer(&bd, nullptr, aliveList[1].ReleaseAndGetAddressOf());

    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.BufferEx.FirstElement = 0;
    srvDesc.BufferEx.NumElements = MAX_PARTICLES;
    device->CreateShaderResourceView(aliveList[0].Get(), &srvDesc,
                                     aliveListSRV[0].ReleaseAndGetAddressOf());
    device->CreateShaderResourceView(aliveList[1].Get(), &srvDesc,
                                     aliveListSRV[1].ReleaseAndGetAddressOf());

    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = MAX_PARTICLES;
    device->CreateUnorderedAccessView(aliveList[0].Get(), &uavDesc,
                                      aliveListUAV[0].ReleaseAndGetAddressOf());
    device->CreateUnorderedAccessView(aliveList[1].Get(), &uavDesc,
                                      aliveListUAV[1].ReleaseAndGetAddressOf());

    // Dead index list:
    std::vector<uint32_t> indicies(MAX_PARTICLES);
    for (uint32_t i = 0; i < MAX_PARTICLES; i++) {
      indicies[i] = i;
    }
    data.pSysMem = indicies.data();
    device->CreateBuffer(&bd, &data, deadList.ReleaseAndGetAddressOf());
    data.pSysMem = nullptr;

    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.BufferEx.FirstElement = 0;
    srvDesc.BufferEx.NumElements = MAX_PARTICLES;
    device->CreateShaderResourceView(deadList.Get(), &srvDesc,
                                     deadListSRV.ReleaseAndGetAddressOf());

    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = MAX_PARTICLES;
    device->CreateUnorderedAccessView(deadList.Get(), &uavDesc,
                                      deadListUAV.ReleaseAndGetAddressOf());

    bd.StructureByteStride = sizeof(Vertex);
    bd.ByteWidth = bd.StructureByteStride * 4 * MAX_PARTICLES;
    bd.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    device->CreateBuffer(&bd, nullptr, vertexBuffer.ReleaseAndGetAddressOf());

    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.BufferEx.FirstElement = 0;
    srvDesc.BufferEx.NumElements = 4 * MAX_PARTICLES;
    device->CreateShaderResourceView(vertexBuffer.Get(), &srvDesc,
                                     vertexBufferSRV.ReleaseAndGetAddressOf());

    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = 4 * MAX_PARTICLES;
    device->CreateUnorderedAccessView(vertexBuffer.Get(), &uavDesc,
                                      vertexBufferUAV.ReleaseAndGetAddressOf());
  }

  if (!counterBuffer) {
    // Particle System statistics:
    ParticleCounters counters = {};
    counters.aliveCount = 0;
    counters.deadCount = MAX_PARTICLES;
    counters.realEmitCount = 0;

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    bd.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA data = {};

    data.pSysMem = &counters;
    bd.ByteWidth = sizeof(counters);
    bd.StructureByteStride = sizeof(counters);
    bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
    device->CreateBuffer(&bd, &data, counterBuffer.ReleaseAndGetAddressOf());
    data.pSysMem = nullptr;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    srvDesc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
    srvDesc.BufferEx.FirstElement = 0;
    srvDesc.BufferEx.NumElements = sizeof(counters) / sizeof(uint32_t);
    device->CreateShaderResourceView(counterBuffer.Get(), &srvDesc,
                                     counterBufferSRV.ReleaseAndGetAddressOf());

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = sizeof(counters) / sizeof(uint32_t);
    device->CreateUnorderedAccessView(
        counterBuffer.Get(), &uavDesc,
        counterBufferUAV.ReleaseAndGetAddressOf());
  }

  if (!indirectBuffers) {
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;

    // Indirect Execution buffer:
    bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS |
                   D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
    bd.ByteWidth = sizeof(IndirectDispatchArgs) + sizeof(IndirectDispatchArgs) +
                   sizeof(IndirectDrawArgsInstanced);
    device->CreateBuffer(&bd, nullptr,
                         indirectBuffers.ReleaseAndGetAddressOf());

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements =
        sizeof(IndirectDispatchArgs) / sizeof(uint32_t);
    device->CreateUnorderedAccessView(
        indirectBuffers.Get(), &uavDesc,
        indirectBuffersUAV.ReleaseAndGetAddressOf());

    // Constant buffer:
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(EmittedParticleCB);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;
    device->CreateBuffer(&bd, nullptr, constantBuffer.ReleaseAndGetAddressOf());
  }
}

uint64_t EmittedParticleSystem::GetMemorySizeInBytes() const {
  if (!particleBuffer) {
    return 0;
  }

  uint64_t retVal = 0;

  auto GetSize = [](const ComPtr<ID3D11Buffer>& buffer) -> uint64_t {
    D3D11_BUFFER_DESC desc = {};
    buffer->GetDesc(&desc);
    return desc.ByteWidth;
  };
  retVal += GetSize(particleBuffer);
  retVal += GetSize(aliveList[0]);
  retVal += GetSize(aliveList[1]);
  retVal += GetSize(deadList);
  retVal += GetSize(counterBuffer);
  retVal += GetSize(indirectBuffers);
  retVal += GetSize(constantBuffer);

  return retVal;
}

void EmittedParticleSystem::UpdateCPU(const Matrix& transform, float dt) {
  this->dt = dt;
  CreateSelfBuffers();

  if (IsPaused() || dt == 0) return;

  emit = std::max(0.0f, emit - std::floor(emit));

  center = transform.Translation();

  emit += (float)count * dt;

  emit += burst;
  burst = 0;

  worldMatrix = transform;

  // Swap CURRENT alivelist with NEW alivelist
  std::swap(aliveList[0], aliveList[1]);
}

void EmittedParticleSystem::Burst(int num) {
  if (IsPaused()) return;

  burst += num;
}

void EmittedParticleSystem::Restart() {
  SetPaused(false);
  counterBuffer.Reset();  // will be recreated
}

void EmittedParticleSystem::UpdateGPU(uint32_t instanceIndex) {
  if (!particleBuffer) {
    return;
  }

  if (!IsPaused() && dt > 0) {
    EmittedParticleCB cb = {};
    cb.xEmitterTransform = worldMatrix.Transpose();
    cb.xEmitterBaseMeshUnormRemap = Matrix::Identity.Transpose();
    cb.xEmitCount = static_cast<uint32_t>(emit);
    cb.xEmitterMeshGeometryOffset = 0;
    cb.xEmitterMeshGeometryCount = 0;
    auto GetRandom = [](float minValue, float maxValue) -> float {
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_real_distribution<float> dis(minValue, maxValue);
      return dis(gen);
    };
    cb.xEmitterRandomness = GetRandom(0.0f, 1.0f);
    cb.xParticleLifeSpan = life;
    cb.xParticleLifeSpanRandomness = random_life;
    cb.xParticleNormalFactor = normal_factor;
    cb.xParticleRandomFactor = random_factor;
    cb.xParticleScaling = scaleX;
    cb.xParticleSize = size;
    cb.xParticleMotionBlurAmount = motionBlurAmount;
    cb.xParticleRotation = rotation * XM_PI * 60;
    cb.xParticleMass = mass;
    cb.xEmitterMaxParticleCount = MAX_PARTICLES;
    cb.xEmitterFixedTimestep = FIXED_TIMESTEP;
    cb.xEmitterRestitution = restitution;
    cb.xEmitterFramesX = std::max(1u, framesX);
    cb.xEmitterFramesY = std::max(1u, framesY);
    cb.xEmitterFrameCount = std::max(1u, frameCount);
    cb.xEmitterFrameStart = frameStart;
    cb.xEmitterTexMul = float2(1.0f / static_cast<float>(cb.xEmitterFramesX),
                               1.0f / static_cast<float>(cb.xEmitterFramesY));
    cb.xEmitterFrameRate = frameRate;
    cb.xParticleGravity = gravity;
    cb.xParticleDrag = drag;
    cb.xParticleVelocity = Vector3::TransformNormal(velocity, worldMatrix);
    cb.xParticleRandomColorFactor = random_color;
    cb.xEmitterLayerMask = layerMask;
    cb.xEmitterInstanceIndex = instanceIndex;

    cb.xEmitterOptions = 0;

    context->UpdateSubresource(constantBuffer.Get(), 0, nullptr, &cb, 0, 0);

    context->CSSetConstantBuffers(__CBUFFERBINDSLOT__EmittedParticleCB__, 1,
                                  constantBuffer.GetAddressOf());

    context->CSSetUnorderedAccessViews(0, 1, particleBufferUAV.GetAddressOf(),
                                       nullptr);
    context->CSSetUnorderedAccessViews(1, 1, aliveListUAV[0].GetAddressOf(),
                                       nullptr);
    context->CSSetUnorderedAccessViews(2, 1, aliveListUAV[1].GetAddressOf(),
                                       nullptr);
    context->CSSetUnorderedAccessViews(3, 1, deadListUAV.GetAddressOf(),
                                       nullptr);
    context->CSSetUnorderedAccessViews(4, 1, counterBufferUAV.GetAddressOf(),
                                       nullptr);
    context->CSSetUnorderedAccessViews(5, 1, indirectBuffersUAV.GetAddressOf(),
                                       nullptr);
    context->CSSetUnorderedAccessViews(6, 1, vertexBufferUAV.GetAddressOf(),
                                       nullptr);

    // kick off updating, set up state
    context->CSSetShader(kickoffUpdateCS.Get(), nullptr, 0);
    context->Dispatch(1, 1, 1);

    // emit the required amount if there are free slots in dead list
    context->CSSetShader(emitCS.Get(), nullptr, 0);
    context->DispatchIndirect(indirectBuffers.Get(),
                              ARGUMENTBUFFER_OFFSET_DISPATCHEMIT);

    context->CSSetShader(simulateCS.Get(), nullptr, 0);
    context->DispatchIndirect(indirectBuffers.Get(),
                              ARGUMENTBUFFER_OFFSET_DISPATCHSIMULATION);

    // finish updating, update draw argument buffer:
    context->CSSetShader(finishUpdateCS.Get(), nullptr, 0);

    ID3D11ShaderResourceView* srvs[] = {counterBufferSRV.Get()};
    context->CSSetShaderResources(0, 1, srvs);

    ID3D11UnorderedAccessView* uavs[] = {indirectBuffersUAV.Get()};
    context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

    context->Dispatch(1, 1, 1);
  };
}

void EmittedParticleSystem::Draw() {
  uint32_t stride = sizeof(Vertex);
  uint32_t offset = 0;
  context->IASetInputLayout(nullptr);
  context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride,
                              &offset);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
  context->VSSetShader(vertexShader.Get(), nullptr, 0);
  context->VSSetConstantBuffers(__CBUFFERBINDSLOT__EmittedParticleCB__, 1,
                                constantBuffer.GetAddressOf());
  ID3D11ShaderResourceView* srvs[] = {particleBufferSRV.Get(),
                                      aliveListSRV[1].Get(),  // NEW aliveList
                                      vertexBufferSRV.Get()};
  context->VSSetShaderResources(0, 3, srvs);
  context->PSSetShader(pixelShader[SIMPLE].Get(), nullptr, 0);
  context->PSSetConstantBuffers(__CBUFFERBINDSLOT__EmittedParticleCB__, 1,
                                constantBuffer.GetAddressOf());

  context->OMSetDepthStencilState(depthStencilState.Get(), 0);
  context->RSSetState(rasterizerState.Get());

  context->DrawInstancedIndirect(indirectBuffers.Get(),
                                 ARGUMENTBUFFER_OFFSET_DRAWPARTICLES);
}

namespace ParticleSystem_Internal {
bool LoadShaders() {
  std::string enginePath;
  if (!Helper::GetEnginePath(enginePath)) return false;

  auto RegisterShaderObjFile = [&enginePath](
                                   const std::string& shaderObjFileName,
                                   const std::string& shaderProfile,
                                   ID3D11DeviceChild** deviceChild) -> bool {
    std::vector<BYTE> byteCode;
    Helper::ReadData(enginePath + "/hlsl/objs/" + shaderObjFileName, byteCode);

    if (shaderProfile == "VS") {
      HRESULT hr = device->CreateVertexShader(
          byteCode.data(), byteCode.size(), nullptr,
          reinterpret_cast<ID3D11VertexShader**>(deviceChild));
      if (FAILED(hr)) return false;
    } else if (shaderProfile == "PS") {
      HRESULT hr = device->CreatePixelShader(
          byteCode.data(), byteCode.size(), nullptr,
          reinterpret_cast<ID3D11PixelShader**>(deviceChild));
      if (FAILED(hr)) return false;
    } else if (shaderProfile == "CS") {
      HRESULT hr = device->CreateComputeShader(
          byteCode.data(), byteCode.size(), nullptr,
          reinterpret_cast<ID3D11ComputeShader**>(deviceChild));
      if (FAILED(hr)) return false;
    }

    return true;
  };

  if (!RegisterShaderObjFile("VS_Particle", "VS",
                             reinterpret_cast<ID3D11DeviceChild**>(
                                 vertexShader.ReleaseAndGetAddressOf())))
    return false;

  if (!RegisterShaderObjFile("PS_Particle_Simple", "PS",
                             reinterpret_cast<ID3D11DeviceChild**>(
                                 pixelShader[EmittedParticleSystem::SIMPLE]
                                     .ReleaseAndGetAddressOf())))
    return false;

  if (!RegisterShaderObjFile("CS_Particle_KickoffUpdate", "CS",
                             reinterpret_cast<ID3D11DeviceChild**>(
                                 kickoffUpdateCS.ReleaseAndGetAddressOf())))
    return false;
  if (!RegisterShaderObjFile("CS_Particle_FinishUpdate", "CS",
                             reinterpret_cast<ID3D11DeviceChild**>(
                                 finishUpdateCS.ReleaseAndGetAddressOf())))
    return false;
  if (!RegisterShaderObjFile("CS_Particle_Emit", "CS",
                             reinterpret_cast<ID3D11DeviceChild**>(
                                 emitCS.ReleaseAndGetAddressOf())))
    return false;
  if (!RegisterShaderObjFile("CS_Particle_Simulate", "CS",
                             reinterpret_cast<ID3D11DeviceChild**>(
                                 simulateCS.ReleaseAndGetAddressOf())))
    return false;

  return true;
}
}  // namespace ParticleSystem_Internal

bool EmittedParticleSystem::InitParticle(ComPtr<ID3D11Device>& device,
                                         ComPtr<ID3D11DeviceContext>& context) {
  my::device = device;
  my::context = context;

  D3D11_RASTERIZER_DESC rasterizerDesc = {};
  rasterizerDesc.FillMode = D3D11_FILL_SOLID;
  rasterizerDesc.CullMode = D3D11_CULL_NONE;
  rasterizerDesc.FrontCounterClockwise = false;
  rasterizerDesc.DepthBias = 0;
  rasterizerDesc.DepthBiasClamp = 0;
  rasterizerDesc.SlopeScaledDepthBias = 0;
  rasterizerDesc.DepthClipEnable = true;
  rasterizerDesc.ScissorEnable = false;

  if (FAILED(device->CreateRasterizerState(
          &rasterizerDesc, rasterizerState.ReleaseAndGetAddressOf())))
    return false;

  D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
  depthStencilDesc.DepthEnable = false;
  depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
  depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
  depthStencilDesc.StencilEnable = false;

  if (FAILED(device->CreateDepthStencilState(
          &depthStencilDesc, depthStencilState.ReleaseAndGetAddressOf())))
    return false;

  if (!ParticleSystem_Internal::LoadShaders()) return false;
}

void EmittedParticleSystem::DeinitParticle() {
  vertexShader.Reset();
  pixelShader[SIMPLE].Reset();
  kickoffUpdateCS.Reset();
  finishUpdateCS.Reset();
  emitCS.Reset();
  simulateCS.Reset();

  rasterizerState.Reset();
  depthStencilState.Reset();

  particleBuffer.Reset();
  aliveList[0].Reset();
  aliveList[1].Reset();
  deadList.Reset();
  counterBuffer.Reset();
  indirectBuffers.Reset();
  constantBuffer.Reset();
  vertexBuffer.Reset();

  particleBufferSRV.Reset();
  aliveListSRV[0].Reset();
  aliveListSRV[1].Reset();
  deadListSRV.Reset();
  counterBufferSRV.Reset();
  constantBufferSRV.Reset();
  vertexBufferSRV.Reset();

  particleBufferUAV.Reset();
  aliveListUAV[0].Reset();
  aliveListUAV[1].Reset();
  deadListUAV.Reset();
  counterBufferUAV.Reset();
  indirectBuffersUAV.Reset();
  constantBufferUAV.Reset();
  vertexBufferUAV.Reset();
}
}  // namespace my