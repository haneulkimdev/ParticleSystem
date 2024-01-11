#include "MyEngineAPI.h"

namespace my {
std::shared_ptr<spdlog::logger> g_apiLogger;

ComPtr<ID3D11Device> g_device;
ComPtr<ID3D11DeviceContext> g_context;

// Resources
ComPtr<ID3D11Texture2D> g_renderTargetBuffer;
ComPtr<ID3D11Texture2D> g_depthStencilBuffer;
ComPtr<ID3D11Buffer> g_particleBuffer;
ComPtr<ID3D11Buffer> g_aliveList[2];
ComPtr<ID3D11Buffer> g_deadList;
ComPtr<ID3D11Buffer> g_counterBuffer;

// Views
ComPtr<ID3D11RenderTargetView> g_renderTargetView;
ComPtr<ID3D11DepthStencilView> g_depthStencilView;
ComPtr<ID3D11ShaderResourceView> g_sharedSRV;
ComPtr<ID3D11ShaderResourceView> g_particlesSRV;
ComPtr<ID3D11UnorderedAccessView> g_particlesUAV;
ComPtr<ID3D11ShaderResourceView> g_aliveListSRV[2];
ComPtr<ID3D11UnorderedAccessView> g_aliveListUAV[2];
ComPtr<ID3D11UnorderedAccessView> g_deadListUAV;
ComPtr<ID3D11UnorderedAccessView> g_counterBufferUAV;

// Shaders
ComPtr<ID3D11ComputeShader> g_particleSystemCS_emit;
ComPtr<ID3D11ComputeShader> g_particleSystemCS_update;
ComPtr<ID3D11VertexShader> g_particleSystemVS;
ComPtr<ID3D11PixelShader> g_particleSystemPS;

// Buffers
ComPtr<ID3D11Buffer> g_frameCB;
ComPtr<ID3D11Buffer> g_particleSystemCB;
ComPtr<ID3D11Buffer> g_statisticsReadbackBuffer;
ComPtr<ID3D11Buffer> g_quadRendererCB;

// States
ComPtr<ID3D11RasterizerState> g_rasterizerState;
ComPtr<ID3D11DepthStencilState> g_depthStencilState;

ParticleSystemCB g_particleSystemData = {};

ParticleCounters g_statistics = {};

const int MAX_PARTICLES = 1000;

uint32_t g_frameCount = 0;

PointLight g_pointLight;

Camera g_camera;

float g_smoothingCoefficient = 10.0f;

Vector3 g_distBoxCenter;
float g_distBoxSize;

D3D11_VIEWPORT g_viewport;

int g_renderTargetWidth;
int g_renderTargetHeight;

auto FailRet = [](const std::string& msg) {
  g_apiLogger->error(msg);
  return false;
};

ParticleCounters GetStatistics() { return g_statistics; }

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

    hr = g_device->CreateBuffer(&bd, nullptr,
                                g_particleSystemCB.ReleaseAndGetAddressOf());

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

  D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
  depthStencilDesc.DepthEnable = true;
  depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
  depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
  depthStencilDesc.StencilEnable = false;

  hr = g_device->CreateDepthStencilState(
      &depthStencilDesc, g_depthStencilState.ReleaseAndGetAddressOf());

  if (FAILED(hr)) FailRet("CreateDepthStencilState Failed.");

  LoadShaders();

  // Build the view matrix.
  Vector3 pos(0.0f, 0.0f, -5.0f);
  Vector3 forward(0.0f, 0.0f, 1.0f);
  Vector3 up(0.0f, 1.0f, 0.0f);

  g_camera.LookAt(pos, pos + forward, up);
  g_camera.UpdateViewMatrix();

  g_pointLight.position = g_camera.GetPosition();
  g_pointLight.color = 0xffffffff;
  g_pointLight.intensity = 1.0f;

  g_distBoxCenter = Vector3(0.0f, 0.0f, 0.0f);
  g_distBoxSize = 2.0f;

  // Particle buffer:
  {
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(Particle) * MAX_PARTICLES;
    bd.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bd.StructureByteStride = sizeof(Particle);

    hr = g_device->CreateBuffer(&bd, nullptr,
                                g_particleBuffer.ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateBuffer Failed.");

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.BufferEx.NumElements = MAX_PARTICLES;

    hr = g_device->CreateShaderResourceView(
        g_particleBuffer.Get(), &srvDesc,
        g_particlesSRV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateShaderResourceView Failed.");

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.NumElements = MAX_PARTICLES;

    hr = g_device->CreateUnorderedAccessView(
        g_particleBuffer.Get(), &uavDesc,
        g_particlesUAV.ReleaseAndGetAddressOf());
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

    hr = g_device->CreateBuffer(&bd, nullptr,
                                g_aliveList[0].ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateBuffer Failed.");

    hr = g_device->CreateBuffer(&bd, nullptr,
                                g_aliveList[1].ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateBuffer Failed.");

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.BufferEx.NumElements = MAX_PARTICLES;

    hr = g_device->CreateShaderResourceView(
        g_aliveList[0].Get(), &srvDesc,
        g_aliveListSRV[0].ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateShaderResourceView Failed.");

    hr = g_device->CreateShaderResourceView(
        g_aliveList[1].Get(), &srvDesc,
        g_aliveListSRV[1].ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateShaderResourceView Failed.");

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.NumElements = MAX_PARTICLES;

    hr = g_device->CreateUnorderedAccessView(
        g_aliveList[0].Get(), &uavDesc,
        g_aliveListUAV[0].ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateUnorderedAccessView Failed.");

    hr = g_device->CreateUnorderedAccessView(
        g_aliveList[1].Get(), &uavDesc,
        g_aliveListUAV[1].ReleaseAndGetAddressOf());
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

    hr = g_device->CreateBuffer(&bd, &initData,
                                g_deadList.ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateBuffer Failed.");

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.NumElements = MAX_PARTICLES;

    hr = g_device->CreateUnorderedAccessView(
        g_deadList.Get(), &uavDesc, g_deadListUAV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateUnorderedAccessView Failed.");
  }

  // Particle System statistics:
  {
    ParticleCounters counters = {};
    counters.aliveCount = 0;
    counters.deadCount = MAX_PARTICLES;
    counters.realEmitCount = 0;

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(counters);
    bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = &counters;

    hr = g_device->CreateBuffer(&bd, &initData,
                                g_counterBuffer.ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateBuffer Failed.");

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.NumElements = sizeof(counters) / sizeof(uint32_t);
    uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

    hr = g_device->CreateUnorderedAccessView(
        g_counterBuffer.Get(), &uavDesc,
        g_counterBufferUAV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateUnorderedAccessView Failed.");
  }

  // Debug information CPU-readback buffer:
  {
    D3D11_BUFFER_DESC bd;
    g_counterBuffer->GetDesc(&bd);
    bd.Usage = D3D11_USAGE_STAGING;
    bd.BindFlags = 0;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    bd.MiscFlags = 0;

    hr = g_device->CreateBuffer(
        &bd, nullptr, g_statisticsReadbackBuffer.ReleaseAndGetAddressOf());
    if (FAILED(hr)) FailRet("CreateBuffer Failed.");
  }

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
                   1.0f, 1000.0f);

  return true;
}

void Update(float dt) {
  {
    FrameCB frameCB = {};
    frameCB.delta_time = dt;
    frameCB.frame_count = g_frameCount++;

    D3D11_MAPPED_SUBRESOURCE mappedResource = {};
    g_context->Map(g_frameCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
                   &mappedResource);
    memcpy(mappedResource.pData, &frameCB, sizeof(frameCB));
  }

  {
    PostRenderer quadPostRenderer = {};
    quadPostRenderer.posCam = g_camera.GetPosition();
    quadPostRenderer.lightColor = g_pointLight.color;
    quadPostRenderer.posLight = g_pointLight.position;
    quadPostRenderer.lightIntensity = g_pointLight.intensity;
    quadPostRenderer.matWS2CS = g_camera.View().Transpose();
    quadPostRenderer.matPS2WS = g_camera.ViewProj().Invert().Transpose();
    quadPostRenderer.rtSize = Vector2(static_cast<float>(g_renderTargetWidth),
                                      static_cast<float>(g_renderTargetHeight));
    quadPostRenderer.smoothingCoefficient = g_smoothingCoefficient;
    quadPostRenderer.deltaTime = dt;
    quadPostRenderer.distBoxCenter = g_distBoxCenter;
    quadPostRenderer.distBoxSize = g_distBoxSize;

    D3D11_MAPPED_SUBRESOURCE mappedResource = {};
    g_context->Map(g_quadRendererCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
                   &mappedResource);
    memcpy(mappedResource.pData, &quadPostRenderer, sizeof(quadPostRenderer));
    g_context->Unmap(g_quadRendererCB.Get(), 0);
  }
}

bool DoTest() {
  // Emission stage
  {
    g_context->CSSetShader(g_particleSystemCS_emit.Get(), nullptr, 0);
    g_context->CSSetConstantBuffers(0, 1, g_frameCB.GetAddressOf());
    g_context->CSSetConstantBuffers(1, 1, g_particleSystemCB.GetAddressOf());
    g_context->CSSetUnorderedAccessViews(0, 1, g_particlesUAV.GetAddressOf(),
                                         nullptr);
    g_context->CSSetUnorderedAccessViews(1, 1, g_aliveListUAV[0].GetAddressOf(),
                                         nullptr);
    g_context->CSSetUnorderedAccessViews(2, 1, g_aliveListUAV[1].GetAddressOf(),
                                         nullptr);
    g_context->CSSetUnorderedAccessViews(3, 1, g_deadListUAV.GetAddressOf(),
                                         nullptr);
    g_context->CSSetUnorderedAccessViews(
        4, 1, g_counterBufferUAV.GetAddressOf(), nullptr);
    g_context->Dispatch(MAX_PARTICLES, 1, 1);

    GPUBarrier();
  }

  // Simulation stage
  {
    g_context->CSSetShader(g_particleSystemCS_update.Get(), nullptr, 0);
    g_context->CSSetConstantBuffers(0, 1, g_frameCB.GetAddressOf());
    g_context->CSSetConstantBuffers(1, 1, g_particleSystemCB.GetAddressOf());
    g_context->CSSetUnorderedAccessViews(0, 1, g_particlesUAV.GetAddressOf(),
                                         nullptr);
    g_context->CSSetUnorderedAccessViews(1, 1, g_aliveListUAV[0].GetAddressOf(),
                                         nullptr);
    g_context->CSSetUnorderedAccessViews(2, 1, g_aliveListUAV[1].GetAddressOf(),
                                         nullptr);
    g_context->CSSetUnorderedAccessViews(3, 1, g_deadListUAV.GetAddressOf(),
                                         nullptr);
    g_context->CSSetUnorderedAccessViews(
        4, 1, g_counterBufferUAV.GetAddressOf(), nullptr);
    g_context->Dispatch(MAX_PARTICLES, 1, 1);

    GPUBarrier();
  }

  g_context->CopyResource(g_statisticsReadbackBuffer.Get(),
                          g_counterBuffer.Get());

  D3D11_MAPPED_SUBRESOURCE mappedResource = {};
  g_context->Map(g_statisticsReadbackBuffer.Get(), 0, D3D11_MAP_READ, 0,
                 &mappedResource);
  memcpy(&g_statistics, mappedResource.pData, sizeof(g_statistics));
  g_context->Unmap(g_statisticsReadbackBuffer.Get(), 0);

  // Rendering stage
  {
    const float clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    g_context->ClearRenderTargetView(g_renderTargetView.Get(), clearColor);

    g_context->OMSetRenderTargets(1, g_renderTargetView.GetAddressOf(),
                                  nullptr);
    g_context->VSSetShader(g_particleSystemVS.Get(), nullptr, 0);
    g_context->PSSetShader(g_particleSystemPS.Get(), nullptr, 0);

    g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    g_context->VSSetShaderResources(0, 1, g_particlesSRV.GetAddressOf());
    g_context->Draw(static_cast<UINT>(MAX_PARTICLES), 0);

    GPUBarrier();

    g_context->Flush();
  }

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
  // States
  g_rasterizerState.Reset();
  g_depthStencilState.Reset();

  // Buffers
  g_frameCB.Reset();
  g_particleSystemCB.Reset();
  g_statisticsReadbackBuffer.Reset();
  g_quadRendererCB.Reset();

  // Shaders
  g_particleSystemCS_emit.Reset();
  g_particleSystemCS_update.Reset();
  g_particleSystemVS.Reset();
  g_particleSystemPS.Reset();

  // Views
  g_renderTargetView.Reset();
  g_depthStencilView.Reset();
  g_sharedSRV.Reset();
  g_particlesSRV.Reset();
  g_particlesUAV.Reset();
  g_aliveListSRV[0].Reset();
  g_aliveListSRV[1].Reset();
  g_aliveListUAV[0].Reset();
  g_aliveListUAV[1].Reset();
  g_deadListUAV.Reset();
  g_counterBufferUAV.Reset();

  // Resources
  g_renderTargetBuffer.Reset();
  g_depthStencilBuffer.Reset();
  g_particleBuffer.Reset();
  g_aliveList[0].Reset();
  g_aliveList[1].Reset();
  g_deadList.Reset();
  g_counterBuffer.Reset();

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
          "CS_ParticleSystem_Emit", "CS",
          reinterpret_cast<ID3D11DeviceChild**>(
              g_particleSystemCS_emit.ReleaseAndGetAddressOf())))
    FailRet("RegisterShaderObjFile Failed.");
  if (!RegisterShaderObjFile(
          "CS_ParticleSystem_Update", "CS",
          reinterpret_cast<ID3D11DeviceChild**>(
              g_particleSystemCS_update.ReleaseAndGetAddressOf())))
    FailRet("RegisterShaderObjFile Failed.");
  if (!RegisterShaderObjFile("VS_ParticleSystem", "VS",
                             reinterpret_cast<ID3D11DeviceChild**>(
                                 g_particleSystemVS.ReleaseAndGetAddressOf())))
    FailRet("RegisterShaderObjFile Failed.");
  if (!RegisterShaderObjFile("PS_ParticleSystem", "PS",
                             reinterpret_cast<ID3D11DeviceChild**>(
                                 g_particleSystemPS.ReleaseAndGetAddressOf())))
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

  ID3D11UnorderedAccessView* nullUAV[D3D11_PS_CS_UAV_REGISTER_COUNT] = {
      nullptr};
  g_context->CSSetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT,
                                       nullUAV, nullptr);
}
}  // namespace my