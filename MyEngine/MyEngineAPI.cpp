#include "MyEngineAPI.h"

std::shared_ptr<spdlog::logger> g_apiLogger;

auto FailRet = [](const std::string& msg) {
  g_apiLogger->error(msg);
  return false;
};

ComPtr<ID3D11Device> g_device;
ComPtr<ID3D11DeviceContext> g_context;

// Resources
ComPtr<ID3D11Texture2D> g_renderTargetBuffer;
ComPtr<ID3D11Texture2D> g_depthStencilBuffer;
ComPtr<ID3D11Buffer> g_frameCB;
ComPtr<ID3D11Buffer> g_quadRendererCB;
ComPtr<ID3D11Buffer> g_sphereVB;
ComPtr<ID3D11Buffer> g_sphereIB;

// Views
ComPtr<ID3D11RenderTargetView> g_renderTargetView;
ComPtr<ID3D11DepthStencilView> g_depthStencilView;
ComPtr<ID3D11ShaderResourceView> g_sharedSRV;

// Shaders
ComPtr<ID3D11VertexShader> g_vertexShader;
ComPtr<ID3D11PixelShader> g_pixelShader;

// States
ComPtr<ID3D11RasterizerState> g_rasterizerState;
ComPtr<ID3D11RasterizerState> g_wireframeRS;
ComPtr<ID3D11DepthStencilState> g_depthStencilState;
ComPtr<ID3D11BlendState> g_blendState;

// Input Layouts
ComPtr<ID3D11InputLayout> g_inputLayout;

namespace my {
bool g_isWireframe = false;

uint32_t g_frameCount = 0;

PointLight g_pointLight;

Camera g_camera;

float g_smoothingCoefficient = 10.0f;

float g_floorHeight = -1.0f;

Vector3 g_distBoxCenter;
float g_distBoxSize;

uint32_t g_sphereIndexCount;

D3D11_VIEWPORT g_viewport;

int g_renderTargetWidth;
int g_renderTargetHeight;

namespace ParticleSystem {
void CreateSelfBuffers() {
  // Particle buffer:
  {
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(Particle) * MAX_PARTICLES;
    bd.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bd.StructureByteStride = sizeof(Particle);

    HRESULT hr = g_device->CreateBuffer(
        &bd, nullptr, particleBuffer.ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateBuffer Failed.");

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.BufferEx.NumElements = MAX_PARTICLES;

    hr = g_device->CreateShaderResourceView(
        particleBuffer.Get(), &srvDesc,
        particleBufferSRV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateShaderResourceView Failed.");

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.NumElements = MAX_PARTICLES;

    hr = g_device->CreateUnorderedAccessView(
        particleBuffer.Get(), &uavDesc,
        particleBufferUAV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateUnorderedAccessView Failed.");
  }

  // Alive index lists (double buffered):
  {
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(uint32_t) * MAX_PARTICLES;
    bd.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bd.StructureByteStride = sizeof(uint32_t);

    HRESULT hr = g_device->CreateBuffer(&bd, nullptr,
                                        aliveList[0].ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateBuffer Failed.");

    hr = g_device->CreateBuffer(&bd, nullptr,
                                aliveList[1].ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateBuffer Failed.");

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.BufferEx.NumElements = MAX_PARTICLES;

    hr = g_device->CreateShaderResourceView(
        aliveList[0].Get(), &srvDesc, aliveListSRV[0].ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateShaderResourceView Failed.");

    hr = g_device->CreateShaderResourceView(
        aliveList[1].Get(), &srvDesc, aliveListSRV[1].ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateShaderResourceView Failed.");

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.NumElements = MAX_PARTICLES;

    hr = g_device->CreateUnorderedAccessView(
        aliveList[0].Get(), &uavDesc, aliveListUAV[0].ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateUnorderedAccessView Failed.");

    hr = g_device->CreateUnorderedAccessView(
        aliveList[1].Get(), &uavDesc, aliveListUAV[1].ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateUnorderedAccessView Failed.");
  }

  // Dead index list:
  {
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(uint32_t) * MAX_PARTICLES;
    bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bd.StructureByteStride = sizeof(uint32_t);

    D3D11_SUBRESOURCE_DATA initData = {};
    std::vector<uint32_t> indices(MAX_PARTICLES);
    for (uint32_t i = 0; i < MAX_PARTICLES; i++) indices[i] = i;
    initData.pSysMem = indices.data();

    HRESULT hr = g_device->CreateBuffer(&bd, &initData,
                                        deadList.ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateBuffer Failed.");

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.NumElements = MAX_PARTICLES;

    hr = g_device->CreateUnorderedAccessView(
        deadList.Get(), &uavDesc, deadListUAV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateUnorderedAccessView Failed.");
  }

  // Particle System statistics:
  {
    ParticleCounters counters = {};
    counters.aliveCount = 0;
    counters.deadCount = MAX_PARTICLES;
    counters.realEmitCount = 0;
    counters.aliveCount_afterSimulation = 0;

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(counters);
    bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = &counters;

    HRESULT hr = g_device->CreateBuffer(&bd, &initData,
                                        counterBuffer.ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateBuffer Failed.");

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.NumElements = sizeof(counters) / sizeof(uint32_t);
    uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

    hr = g_device->CreateUnorderedAccessView(
        counterBuffer.Get(), &uavDesc,
        counterBufferUAV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateUnorderedAccessView Failed.");
  }

  // Debug information CPU-readback buffer:
  {
    D3D11_BUFFER_DESC bd;
    counterBuffer->GetDesc(&bd);
    bd.Usage = D3D11_USAGE_STAGING;
    bd.BindFlags = 0;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    bd.MiscFlags = 0;

    HRESULT hr = g_device->CreateBuffer(
        &bd, nullptr, statisticsReadbackBuffer.ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateBuffer Failed.");
  }
}

void UpdateGPU() {
  // Update emitter properties constant buffer.
  {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);

    ParticleSystemCB particleSystemCB = {};
    particleSystemCB.xEmitCount = static_cast<uint32_t>(emit);
    particleSystemCB.xEmitterRandomness = dis(gen);
    particleSystemCB.xParticleLifeSpan = particleEmitter.life;
    particleSystemCB.xParticleLifeSpanRandomness = particleEmitter.random_life;
    particleSystemCB.xParticleNormalFactor = particleEmitter.normal_factor;
    particleSystemCB.xParticleRandomFactor = particleEmitter.random_factor;
    particleSystemCB.xParticleScaling = particleEmitter.scale;
    particleSystemCB.xParticleSize = particleEmitter.size;
    particleSystemCB.xParticleRotation = particleEmitter.rotation * XM_PI * 60;
    particleSystemCB.xParticleMass = particleEmitter.mass;
    particleSystemCB.xEmitterMaxParticleCount = MAX_PARTICLES;
    particleSystemCB.xEmitterRestitution = particleEmitter.restitution;
    particleSystemCB.xParticleGravity =
        float3(particleEmitter.gravity[0], particleEmitter.gravity[1],
               particleEmitter.gravity[2]);
    particleSystemCB.xParticleDrag = particleEmitter.drag;
    particleSystemCB.xParticleVelocity =
        float3(particleEmitter.velocity[0], particleEmitter.velocity[1],
               particleEmitter.velocity[2]);
    particleSystemCB.xParticleRandomColorFactor = particleEmitter.random_color;

    D3D11_MAPPED_SUBRESOURCE mappedResource = {};
    g_context->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
                   &mappedResource);
    memcpy(mappedResource.pData, &particleSystemCB, sizeof(particleSystemCB));
    g_context->Unmap(constantBuffer.Get(), 0);
  }

  // kick off updating, set up state
  {
    g_context->CSSetShader(ParticleSystem::kickoffUpdateCS.Get(), nullptr, 0);
    g_context->CSSetConstantBuffers(1, 1, constantBuffer.GetAddressOf());
    g_context->CSSetUnorderedAccessViews(4, 1, counterBufferUAV.GetAddressOf(),
                                         nullptr);
    g_context->Dispatch(1, 1, 1);

    GPUBarrier();
  }

  // emit the required amount if there are free slots in dead list
  {
    g_context->CSSetShader(ParticleSystem::emitCS.Get(), nullptr, 0);
    g_context->CSSetConstantBuffers(0, 1, g_frameCB.GetAddressOf());
    g_context->CSSetConstantBuffers(1, 1, constantBuffer.GetAddressOf());
    g_context->CSSetUnorderedAccessViews(0, 1, particleBufferUAV.GetAddressOf(),
                                         nullptr);
    g_context->CSSetUnorderedAccessViews(1, 1, aliveListUAV[0].GetAddressOf(),
                                         nullptr);
    g_context->CSSetUnorderedAccessViews(2, 1, aliveListUAV[1].GetAddressOf(),
                                         nullptr);
    g_context->CSSetUnorderedAccessViews(3, 1, deadListUAV.GetAddressOf(),
                                         nullptr);
    g_context->CSSetUnorderedAccessViews(4, 1, counterBufferUAV.GetAddressOf(),
                                         nullptr);
    g_context->Dispatch(MAX_PARTICLES, 1, 1);

    GPUBarrier();
  }

  // update CURRENT alive list, write NEW alive list
  {
    g_context->CSSetShader(ParticleSystem::simulateCS.Get(), nullptr, 0);
    g_context->CSSetConstantBuffers(0, 1, g_frameCB.GetAddressOf());
    g_context->CSSetConstantBuffers(1, 1, constantBuffer.GetAddressOf());
    g_context->CSSetConstantBuffers(2, 1, g_quadRendererCB.GetAddressOf());
    g_context->CSSetUnorderedAccessViews(0, 1, particleBufferUAV.GetAddressOf(),
                                         nullptr);
    g_context->CSSetUnorderedAccessViews(1, 1, aliveListUAV[0].GetAddressOf(),
                                         nullptr);
    g_context->CSSetUnorderedAccessViews(2, 1, aliveListUAV[1].GetAddressOf(),
                                         nullptr);
    g_context->CSSetUnorderedAccessViews(3, 1, deadListUAV.GetAddressOf(),
                                         nullptr);
    g_context->CSSetUnorderedAccessViews(4, 1, counterBufferUAV.GetAddressOf(),
                                         nullptr);
    g_context->Dispatch(MAX_PARTICLES, 1, 1);

    GPUBarrier();
  }

  // Swap CURRENT alivelist with NEW alivelist
  std::swap(aliveList[0], aliveList[1]);
  std::swap(aliveListSRV[0], aliveListSRV[1]);
  std::swap(aliveListUAV[0], aliveListUAV[1]);

  // Statistics is copied to readback:
  g_context->CopyResource(statisticsReadbackBuffer.Get(), counterBuffer.Get());

  // Read back statistics
  D3D11_MAPPED_SUBRESOURCE mappedResource = {};
  g_context->Map(statisticsReadbackBuffer.Get(), 0, D3D11_MAP_READ, 0,
                 &mappedResource);
  memcpy(&statistics, mappedResource.pData, sizeof(statistics));
  g_context->Unmap(statisticsReadbackBuffer.Get(), 0);
}

void UpdateCPU(float dt) {
  emit = std::max(0.0f, emit - std::floor(emit));

  emit += particleEmitter.count * dt;
}

void Draw() {
  g_context->VSSetShader(vertexShader.Get(), nullptr, 0);
  g_context->GSSetShader(geometryShader.Get(), nullptr, 0);
  g_context->PSSetShader(pixelShader.Get(), nullptr, 0);
  g_context->OMSetBlendState(g_blendState.Get(), nullptr, 0xffffffff);

  g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
  g_context->VSSetConstantBuffers(2, 1, g_quadRendererCB.GetAddressOf());
  g_context->VSSetShaderResources(0, 1, particleBufferSRV.GetAddressOf());
  g_context->GSSetShaderResources(0, 1, particleBufferSRV.GetAddressOf());
  g_context->Draw(static_cast<UINT>(MAX_PARTICLES), 0);

  GPUBarrier();

  g_context->Flush();
}

ParticleEmitter* GetParticleEmitter() { return &particleEmitter; }

ParticleCounters GetStatistics() { return statistics; }
}  // namespace ParticleSystem

void SetWireframe(bool value) { g_isWireframe = value; }

bool IsWireframe() { return g_isWireframe; }

void SetFloorHeight(float value) { g_floorHeight = value; }

float GetFloorHeight() { return g_floorHeight; }

bool InitEngine(std::shared_ptr<spdlog::logger> spdlogPtr) {
  g_apiLogger = spdlogPtr;

  // Create the device and device context.

  uint32_t createDeviceFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
  createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

  D3D_FEATURE_LEVEL featureLevel;
  HRESULT hr = D3D11CreateDevice(
      nullptr,  // default adapter
      D3D_DRIVER_TYPE_HARDWARE,
      nullptr,                        // no software device
      createDeviceFlags, nullptr, 0,  // default feature level array
      D3D11_SDK_VERSION, g_device.ReleaseAndGetAddressOf(), &featureLevel,
      g_context.ReleaseAndGetAddressOf());

  if (FAILED(hr)) FailRet("D3D11CreateDevice Failed.");

  if (featureLevel != D3D_FEATURE_LEVEL_11_0)
    FailRet("Direct3D Feature Level 11 unsupported.");

  // Particle System constant buffer:
  {
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(ParticleSystemCB);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags = 0;

    hr = g_device->CreateBuffer(
        &bd, nullptr, ParticleSystem::constantBuffer.ReleaseAndGetAddressOf());

    if (FAILED(hr)) FailRet("CreateBuffer Failed.");
  }

  // Frame constant buffer:
  {
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(FrameCB);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags = 0;

    hr = g_device->CreateBuffer(&bd, nullptr,
                                g_frameCB.ReleaseAndGetAddressOf());

    if (FAILED(hr)) FailRet("CreateBuffer Failed.");
  }

  // PostRenderer constant buffer:
  {
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(PostRenderer);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags = 0;

    hr = g_device->CreateBuffer(&bd, nullptr,
                                g_quadRendererCB.ReleaseAndGetAddressOf());

    if (FAILED(hr)) FailRet("CreateBuffer Failed.");
  }

  D3D11_RASTERIZER_DESC rasterizerDesc = {};
  rasterizerDesc.FillMode = D3D11_FILL_SOLID;
  rasterizerDesc.CullMode = D3D11_CULL_BACK;
  rasterizerDesc.FrontCounterClockwise = false;
  rasterizerDesc.DepthBias = 0;
  rasterizerDesc.DepthBiasClamp = 0;
  rasterizerDesc.SlopeScaledDepthBias = 0;
  rasterizerDesc.DepthClipEnable = true;
  rasterizerDesc.ScissorEnable = false;
  rasterizerDesc.MultisampleEnable = false;
  rasterizerDesc.AntialiasedLineEnable = false;

  hr = g_device->CreateRasterizerState(
      &rasterizerDesc, g_rasterizerState.ReleaseAndGetAddressOf());

  if (FAILED(hr)) FailRet("CreateRasterizerState Failed.");

  rasterizerDesc.FillMode = D3D11_FILL_WIREFRAME;

  hr = g_device->CreateRasterizerState(&rasterizerDesc,
                                       g_wireframeRS.ReleaseAndGetAddressOf());

  if (FAILED(hr)) FailRet("CreateRasterizerState Failed.");

  D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
  depthStencilDesc.DepthEnable = true;
  depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
  depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
  depthStencilDesc.StencilEnable = false;

  hr = g_device->CreateDepthStencilState(
      &depthStencilDesc, g_depthStencilState.ReleaseAndGetAddressOf());

  if (FAILED(hr)) FailRet("CreateDepthStencilState Failed.");

  D3D11_BLEND_DESC blendDesc = {};
  blendDesc.AlphaToCoverageEnable = false;
  blendDesc.IndependentBlendEnable = false;
  blendDesc.RenderTarget[0].BlendEnable = true;
  blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
  blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
  blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
  blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
  blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  blendDesc.RenderTarget[0].RenderTargetWriteMask =
      D3D11_COLOR_WRITE_ENABLE_ALL;

  hr = g_device->CreateBlendState(&blendDesc,
                                  g_blendState.ReleaseAndGetAddressOf());

  if (FAILED(hr)) FailRet("CreateBlendState Failed.");

  LoadShaders();

  ParticleSystem::CreateSelfBuffers();

  BuildGeometryBuffers();

  // Build the view matrix.
  Vector3 pos(0.0f, 0.0f, -1.0f);
  Vector3 forward(0.0f, 0.0f, 1.0f);
  Vector3 up(0.0f, 1.0f, 0.0f);

  g_camera.LookAt(pos, pos + forward, up);
  g_camera.UpdateViewMatrix();

  g_pointLight.position = g_camera.GetPosition();
  g_pointLight.color = 0xffffffff;
  g_pointLight.intensity = 1.0f;

  g_distBoxCenter = Vector3(0.0f, 0.0f, 0.0f);
  g_distBoxSize = 2.0f;

  return true;
}

bool SetRenderTargetSize(int w, int h) {
  g_renderTargetWidth = w;
  g_renderTargetHeight = h;

  g_sharedSRV.Reset();

  D3D11_TEXTURE2D_DESC renderTargetBufferDesc = {};
  renderTargetBufferDesc.Width = g_renderTargetWidth;
  renderTargetBufferDesc.Height = g_renderTargetHeight;
  renderTargetBufferDesc.MipLevels = 0;
  renderTargetBufferDesc.ArraySize = 1;
  renderTargetBufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  renderTargetBufferDesc.SampleDesc.Count = 1;
  renderTargetBufferDesc.SampleDesc.Quality = 0;
  renderTargetBufferDesc.Usage = D3D11_USAGE_DEFAULT;
  renderTargetBufferDesc.BindFlags =
      D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
  renderTargetBufferDesc.CPUAccessFlags = 0;
  renderTargetBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

  HRESULT hr =
      g_device->CreateTexture2D(&renderTargetBufferDesc, nullptr,
                                g_renderTargetBuffer.ReleaseAndGetAddressOf());

  if (FAILED(hr)) FailRet("CreateTexture2D Failed.");

  D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc = {};

  renderTargetViewDesc.Format = renderTargetBufferDesc.Format;
  renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
  renderTargetViewDesc.Texture2D.MipSlice = 0;

  hr = g_device->CreateRenderTargetView(
      g_renderTargetBuffer.Get(), &renderTargetViewDesc,
      g_renderTargetView.ReleaseAndGetAddressOf());

  if (FAILED(hr)) FailRet("CreateRenderTargetView Failed.");

  // Create the depth/stencil buffer and view.

  D3D11_TEXTURE2D_DESC depthStencilBufferDesc = {};
  depthStencilBufferDesc.Width = g_renderTargetWidth;
  depthStencilBufferDesc.Height = g_renderTargetHeight;
  depthStencilBufferDesc.MipLevels = 0;
  depthStencilBufferDesc.ArraySize = 1;
  depthStencilBufferDesc.Format = DXGI_FORMAT_D32_FLOAT;
  depthStencilBufferDesc.SampleDesc.Count = 1;
  depthStencilBufferDesc.SampleDesc.Quality = 0;
  depthStencilBufferDesc.Usage = D3D11_USAGE_DEFAULT;
  depthStencilBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
  depthStencilBufferDesc.CPUAccessFlags = 0;
  depthStencilBufferDesc.MiscFlags = 0;

  hr = (g_device->CreateTexture2D(
      &depthStencilBufferDesc, nullptr,
      g_depthStencilBuffer.ReleaseAndGetAddressOf()));

  if (FAILED(hr)) FailRet("CreateTexture2D Failed.");

  hr = (g_device->CreateDepthStencilView(
      g_depthStencilBuffer.Get(), nullptr,
      g_depthStencilView.ReleaseAndGetAddressOf()));

  if (FAILED(hr)) FailRet("CreateDepthStencilView Failed.");

  // Bind the render target view and depth/stencil view to the pipeline.

  g_context->OMSetRenderTargets(1, g_renderTargetView.GetAddressOf(),
                                g_depthStencilView.Get());

  // Set the viewport transform.

  g_viewport.TopLeftX = 0.0f;
  g_viewport.TopLeftY = 0.0f;
  g_viewport.Width = static_cast<float>(g_renderTargetWidth);
  g_viewport.Height = static_cast<float>(g_renderTargetHeight);
  g_viewport.MinDepth = 0.0f;
  g_viewport.MaxDepth = 1.0f;

  g_context->RSSetViewports(1, &g_viewport);

  g_camera.SetLens(0.25f * XM_PI,
                   static_cast<float>(g_renderTargetWidth) /
                       static_cast<float>(g_renderTargetHeight),
                   0.1f, 1000.0f);

  return true;
}

void Update(float dt) {
  dt *= 0.5f;

  ParticleSystem::UpdateCPU(dt);

  // Update frame constant buffer.
  {
    FrameCB frameCB = {};
    frameCB.delta_time = dt;
    frameCB.frame_count = g_frameCount++;

    D3D11_MAPPED_SUBRESOURCE mappedResource = {};
    g_context->Map(g_frameCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
                   &mappedResource);
    memcpy(mappedResource.pData, &frameCB, sizeof(frameCB));
    g_context->Unmap(g_frameCB.Get(), 0);
  }

  // Update post renderer constant buffer.
  {
    PostRenderer quadRenderer = {};
    quadRenderer.posCam = g_camera.GetPosition();
    quadRenderer.lightColor = g_pointLight.color;
    quadRenderer.posLight = g_pointLight.position;
    quadRenderer.lightIntensity = g_pointLight.intensity;
    quadRenderer.matWS2PS = g_camera.ViewProj().Transpose();
    quadRenderer.matPS2WS = g_camera.ViewProj().Invert().Transpose();
    quadRenderer.rtSize = Vector2(static_cast<float>(g_renderTargetWidth),
                                  static_cast<float>(g_renderTargetHeight));
    quadRenderer.smoothingCoefficient = g_smoothingCoefficient;
    quadRenderer.floorHeight = g_floorHeight;
    quadRenderer.distBoxCenter = g_distBoxCenter;
    quadRenderer.distBoxSize = g_distBoxSize;

    D3D11_MAPPED_SUBRESOURCE mappedResource = {};
    g_context->Map(g_quadRendererCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
                   &mappedResource);
    memcpy(mappedResource.pData, &quadRenderer, sizeof(quadRenderer));
    g_context->Unmap(g_quadRendererCB.Get(), 0);
  }
}

bool DoTest() {
  const float clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  g_context->ClearRenderTargetView(g_renderTargetView.Get(), clearColor);

  g_context->OMSetRenderTargets(1, g_renderTargetView.GetAddressOf(), nullptr);

  if (g_isWireframe)
    g_context->RSSetState(g_wireframeRS.Get());
  else
    g_context->RSSetState(g_rasterizerState.Get());

  g_context->OMSetDepthStencilState(g_depthStencilState.Get(), 0);

  DrawSphere();

  ParticleSystem::UpdateGPU();
  ParticleSystem::Draw();

  return true;
}

bool GetDX11SharedRenderTarget(ID3D11Device* dx11ImGuiDevice,
                               ID3D11ShaderResourceView** sharedSRV, int& w,
                               int& h) {
  w = g_renderTargetWidth;
  h = g_renderTargetHeight;

  *sharedSRV = nullptr;

  if (!g_device) return FailRet("Device not initialized.");

  if (!g_sharedSRV) {
    ComPtr<IDXGIResource> DXGIResource;

    HRESULT hr = g_renderTargetBuffer->QueryInterface(
        __uuidof(IDXGIResource),
        reinterpret_cast<void**>(DXGIResource.ReleaseAndGetAddressOf()));

    if (FAILED(hr)) FailRet("QueryInterface Failed.");

    HANDLE sharedHandle;

    hr = DXGIResource->GetSharedHandle(&sharedHandle);
    DXGIResource.Reset();

    if (FAILED(hr)) FailRet("GetSharedHandle Failed.");

    ComPtr<ID3D11Texture2D> texture;

    hr = dx11ImGuiDevice->OpenSharedResource(
        sharedHandle, __uuidof(ID3D11Texture2D),
        reinterpret_cast<void**>(texture.ReleaseAndGetAddressOf()));

    if (FAILED(hr)) FailRet("OpenSharedResource Failed.");

    D3D11_TEXTURE2D_DESC desc = {};
    texture->GetDesc(&desc);

    // Create texture view
    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MipLevels = desc.MipLevels;
    SRVDesc.Texture2D.MostDetailedMip = 0;

    dx11ImGuiDevice->CreateShaderResourceView(texture.Get(), &SRVDesc,
                                              g_sharedSRV.GetAddressOf());
    texture.Reset();

    if (FAILED(hr)) FailRet("CreateShaderResourceView Failed.");
  }

  *sharedSRV = g_sharedSRV.Get();

  return true;
}

void DeinitEngine() {
  ParticleSystem::statisticsReadbackBuffer.Reset();

  ParticleSystem::particleBuffer.Reset();
  ParticleSystem::aliveList[0].Reset();
  ParticleSystem::aliveList[1].Reset();
  ParticleSystem::deadList.Reset();
  ParticleSystem::counterBuffer.Reset();
  ParticleSystem::constantBuffer.Reset();
  ParticleSystem::particleBufferSRV.Reset();
  ParticleSystem::particleBufferUAV.Reset();
  ParticleSystem::aliveListSRV[0].Reset();
  ParticleSystem::aliveListSRV[1].Reset();
  ParticleSystem::aliveListUAV[0].Reset();
  ParticleSystem::aliveListUAV[1].Reset();
  ParticleSystem::deadListUAV.Reset();
  ParticleSystem::counterBufferUAV.Reset();

  ParticleSystem::vertexShader.Reset();
  ParticleSystem::geometryShader.Reset();
  ParticleSystem::pixelShader.Reset();
  ParticleSystem::kickoffUpdateCS.Reset();
  ParticleSystem::emitCS.Reset();
  ParticleSystem::simulateCS.Reset();

  // Input Layouts
  g_inputLayout.Reset();

  // States
  g_rasterizerState.Reset();
  g_wireframeRS.Reset();
  g_depthStencilState.Reset();
  g_blendState.Reset();

  // Shaders
  g_vertexShader.Reset();
  g_pixelShader.Reset();

  // Views
  g_renderTargetView.Reset();
  g_depthStencilView.Reset();
  g_sharedSRV.Reset();

  // Resources
  g_renderTargetBuffer.Reset();
  g_depthStencilBuffer.Reset();
  g_frameCB.Reset();
  g_quadRendererCB.Reset();
  g_sphereVB.Reset();
  g_sphereIB.Reset();

  g_context.Reset();
  g_device.Reset();
}

bool LoadShaders() {
  std::string enginePath;
  if (!Helper::GetEnginePath(enginePath))
    return FailRet("Failure to Read Engine Path.");

  auto RegisterShaderObjFile = [&enginePath](
                                   const std::string& shaderObjFileName,
                                   const std::string& shaderProfile,
                                   ID3D11DeviceChild** deviceChild) -> bool {
    std::vector<BYTE> byteCode;
    Helper::ReadData(enginePath + "/hlsl/objs/" + shaderObjFileName, byteCode);

    if (shaderProfile == "VS") {
      HRESULT hr = g_device->CreateVertexShader(
          byteCode.data(), byteCode.size(), nullptr,
          reinterpret_cast<ID3D11VertexShader**>(deviceChild));
      if (FAILED(hr)) FailRet("CreateVertexShader Failed.");

      D3D11_INPUT_ELEMENT_DESC inputLayoutDesc[] = {
          {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
           D3D11_INPUT_PER_VERTEX_DATA, 0},
      };

      hr = g_device->CreateInputLayout(inputLayoutDesc, 1, byteCode.data(),
                                       byteCode.size(),
                                       g_inputLayout.ReleaseAndGetAddressOf());
      if (FAILED(hr)) FailRet("CreateInputLayout Failed.");

    } else if (shaderProfile == "GS") {
      HRESULT hr = g_device->CreateGeometryShader(
          byteCode.data(), byteCode.size(), nullptr,
          reinterpret_cast<ID3D11GeometryShader**>(deviceChild));
      if (FAILED(hr)) FailRet("CreateGeometryShader Failed.");
    } else if (shaderProfile == "PS") {
      HRESULT hr = g_device->CreatePixelShader(
          byteCode.data(), byteCode.size(), nullptr,
          reinterpret_cast<ID3D11PixelShader**>(deviceChild));
      if (FAILED(hr)) FailRet("CreatePixelShader Failed.");
    } else if (shaderProfile == "CS") {
      HRESULT hr = g_device->CreateComputeShader(
          byteCode.data(), byteCode.size(), nullptr,
          reinterpret_cast<ID3D11ComputeShader**>(deviceChild));
      if (FAILED(hr)) FailRet("CreateComputeShader Failed.");
    } else {
      return FailRet("Unknown Shader Profile.");
    }

    return true;
  };

  if (!RegisterShaderObjFile(
          "VS_ParticleSystem", "VS",
          reinterpret_cast<ID3D11DeviceChild**>(
              ParticleSystem::vertexShader.ReleaseAndGetAddressOf())))
    FailRet("RegisterShaderObjFile Failed.");
  if (!RegisterShaderObjFile(
          "GS_ParticleSystem", "GS",
          reinterpret_cast<ID3D11DeviceChild**>(
              ParticleSystem::geometryShader.ReleaseAndGetAddressOf())))
    FailRet("RegisterShaderObjFile Failed.");
  if (!RegisterShaderObjFile(
          "PS_ParticleSystem", "PS",
          reinterpret_cast<ID3D11DeviceChild**>(
              ParticleSystem::pixelShader.ReleaseAndGetAddressOf())))
    FailRet("RegisterShaderObjFile Failed.");

  if (!RegisterShaderObjFile(
          "CS_ParticleSystem_KickoffUpdate", "CS",
          reinterpret_cast<ID3D11DeviceChild**>(
              ParticleSystem::kickoffUpdateCS.ReleaseAndGetAddressOf())))
    FailRet("RegisterShaderObjFile Failed.");
  if (!RegisterShaderObjFile(
          "CS_ParticleSystem_Emit", "CS",
          reinterpret_cast<ID3D11DeviceChild**>(
              ParticleSystem::emitCS.ReleaseAndGetAddressOf())))
    FailRet("RegisterShaderObjFile Failed.");
  if (!RegisterShaderObjFile(
          "CS_ParticleSystem_Simulate", "CS",
          reinterpret_cast<ID3D11DeviceChild**>(
              ParticleSystem::simulateCS.ReleaseAndGetAddressOf())))
    FailRet("RegisterShaderObjFile Failed.");

  if (!RegisterShaderObjFile("VS_Default", "VS",
                             reinterpret_cast<ID3D11DeviceChild**>(
                                 g_vertexShader.ReleaseAndGetAddressOf())))
    FailRet("RegisterShaderObjFile Failed.");
  if (!RegisterShaderObjFile("PS_Default", "PS",
                             reinterpret_cast<ID3D11DeviceChild**>(
                                 g_pixelShader.ReleaseAndGetAddressOf())))
    FailRet("RegisterShaderObjFile Failed.");

  return true;
}

void GPUBarrier() {
  ID3D11ShaderResourceView*
      nullSRV[D3D11_COMMONSHADER_INPUT_RESOURCE_REGISTER_COUNT] = {nullptr};
  g_context->CSSetShaderResources(
      0, D3D11_COMMONSHADER_INPUT_RESOURCE_REGISTER_COUNT, nullSRV);
  g_context->VSSetShaderResources(
      0, D3D11_COMMONSHADER_INPUT_RESOURCE_REGISTER_COUNT, nullSRV);
  g_context->GSSetShaderResources(
      0, D3D11_COMMONSHADER_INPUT_RESOURCE_REGISTER_COUNT, nullSRV);

  ID3D11UnorderedAccessView* nullUAV[D3D11_PS_CS_UAV_REGISTER_COUNT] = {
      nullptr};
  g_context->CSSetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT,
                                       nullUAV, nullptr);
}

void DrawSphere() {
  g_context->IASetInputLayout(g_inputLayout.Get());
  g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  uint32_t stride = sizeof(Vertex);
  uint32_t offset = 0;

  g_context->IASetVertexBuffers(0, 1, g_sphereVB.GetAddressOf(), &stride,
                                &offset);
  g_context->IASetIndexBuffer(g_sphereIB.Get(), DXGI_FORMAT_R32_UINT, 0);

  g_context->VSSetShader(g_vertexShader.Get(), nullptr, 0);
  g_context->VSSetConstantBuffers(2, 1, g_quadRendererCB.GetAddressOf());
  g_context->PSSetShader(g_pixelShader.Get(), nullptr, 0);

  g_context->DrawIndexed(g_sphereIndexCount, 0, 0);

  g_context->Flush();
}

void BuildGeometryBuffers() {
  GeometryGenerator geoGen;
  auto sphere = geoGen.CreateSphere(0.1f, 20, 20);

  g_sphereIndexCount = static_cast<uint32_t>(sphere.indices.size());

  std::vector<Vertex> vertices(sphere.vertices.size());
  for (size_t i = 0; i < sphere.vertices.size(); i++) {
    vertices[i].position = sphere.vertices[i].position;
  }

  D3D11_BUFFER_DESC vbd = {};
  vbd.Usage = D3D11_USAGE_IMMUTABLE;
  vbd.ByteWidth = sizeof(Vertex) * static_cast<UINT>(vertices.size());
  vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  vbd.CPUAccessFlags = 0;
  vbd.MiscFlags = 0;

  D3D11_SUBRESOURCE_DATA vinitData = {};
  vinitData.pSysMem = &vertices[0];

  HRESULT hr = g_device->CreateBuffer(&vbd, &vinitData,
                                      g_sphereVB.ReleaseAndGetAddressOf());
  if (FAILED(hr)) FailRet("CreateBuffer Failed.");

  D3D11_BUFFER_DESC ibd = {};
  ibd.Usage = D3D11_USAGE_IMMUTABLE;
  ibd.ByteWidth = sizeof(uint32_t) * g_sphereIndexCount;
  ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
  ibd.CPUAccessFlags = 0;
  ibd.MiscFlags = 0;

  D3D11_SUBRESOURCE_DATA iinitData = {};
  iinitData.pSysMem = &sphere.indices[0];

  hr = g_device->CreateBuffer(&ibd, &iinitData,
                              g_sphereIB.ReleaseAndGetAddressOf());
  if (FAILED(hr)) FailRet("CreateBuffer Failed.");
}
}  // namespace my