#include "MyEngineAPI.h"

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

struct PointLight {
  float3 position;
  float intensity;
  uint color;
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
std::shared_ptr<spdlog::logger> g_apiLogger;

ComPtr<ID3D11Device> g_device;
ComPtr<ID3D11DeviceContext> g_context;

// Resources
ComPtr<ID3D11Texture2D> g_renderTargetBuffer;
ComPtr<ID3D11Texture2D> g_depthStencilBuffer;
ComPtr<ID3D11Buffer> g_particlesGPU;
ComPtr<ID3D11Buffer> g_particlesStagingGPU;

// Views
ComPtr<ID3D11RenderTargetView> g_renderTargetView;
ComPtr<ID3D11DepthStencilView> g_depthStencilView;
ComPtr<ID3D11ShaderResourceView> g_sharedSRV;
ComPtr<ID3D11ShaderResourceView> g_particlesSRV;
ComPtr<ID3D11UnorderedAccessView> g_particlesUAV;

// Shaders
ComPtr<ID3D11VertexShader> g_vertexShader;
ComPtr<ID3D11PixelShader> g_pixelShader;

// Buffers
ComPtr<ID3D11Buffer> g_quadRendererCB;

// States
ComPtr<ID3D11RasterizerState> g_rasterizerState;
ComPtr<ID3D11DepthStencilState> g_depthStencilState;

// InputLayouts
ComPtr<ID3D11InputLayout> g_inputLayout;

std::vector<Particle> g_particlesCPU;

const int MAX_PARTICLES = 2048;

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

  D3D11_BUFFER_DESC constantBufferDesc = {};
  constantBufferDesc.ByteWidth = sizeof(PostRenderer);
  constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
  constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  constantBufferDesc.MiscFlags = 0;

  hr = g_device->CreateBuffer(&constantBufferDesc, nullptr,
                              g_quadRendererCB.ReleaseAndGetAddressOf());

  if (FAILED(hr)) FailRet("CreateBuffer Failed.");

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

  g_particlesCPU.resize(MAX_PARTICLES);

  std::vector<Vector3> rainbow = {
      {1.0f, 0.0f, 0.0f},   // Red
      {1.0f, 0.65f, 0.0f},  // Orange
      {1.0f, 1.0f, 0.0f},   // Yellow
      {0.0f, 1.0f, 0.0f},   // Green
      {0.0f, 0.0f, 1.0f},   // Blue
      {0.3f, 0.0f, 0.5f},   // Indigo
      {0.5f, 0.0f, 1.0f}    // Violet/Purple
  };

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> dp(-1.0f, 1.0f);
  std::uniform_int_distribution<size_t> dc(0, rainbow.size() - 1);
  for (auto& p : g_particlesCPU) {
    p.position = Vector3(dp(gen), dp(gen), 1.0f);
    p.color = rainbow[dc(gen)];
    p.radius = (dp(gen) + 1.3f) * 0.02f;
    p.life = -1.0f;
  }

  D3D11_BUFFER_DESC bd = {};
  bd.Usage = D3D11_USAGE_DEFAULT;
  bd.ByteWidth = sizeof(Particle) * MAX_PARTICLES;
  bd.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
  bd.CPUAccessFlags = 0;
  bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
  bd.StructureByteStride = sizeof(Particle);

  D3D11_SUBRESOURCE_DATA initData = {};
  initData.pSysMem = g_particlesCPU.data();

  hr = g_device->CreateBuffer(&bd, &initData,
                              g_particlesGPU.ReleaseAndGetAddressOf());
  if (FAILED(hr)) FailRet("CreateBuffer Failed.");

  D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Format = DXGI_FORMAT_UNKNOWN;
  srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
  srvDesc.BufferEx.NumElements = MAX_PARTICLES;

  hr = g_device->CreateShaderResourceView(
      g_particlesGPU.Get(), &srvDesc, g_particlesSRV.ReleaseAndGetAddressOf());
  if (FAILED(hr)) FailRet("CreateShaderResourceView Failed.");

  D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
  uavDesc.Format = DXGI_FORMAT_UNKNOWN;
  uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
  uavDesc.Buffer.NumElements = MAX_PARTICLES;

  hr = g_device->CreateUnorderedAccessView(
      g_particlesGPU.Get(), &uavDesc, g_particlesUAV.ReleaseAndGetAddressOf());
  if (FAILED(hr)) FailRet("CreateUnorderedAccessView Failed.");

  bd.Usage = D3D11_USAGE_STAGING;
  bd.BindFlags = 0;
  bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;

  hr = g_device->CreateBuffer(&bd, &initData,
                              g_particlesStagingGPU.ReleaseAndGetAddressOf());

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

  dt *= 0.5f;

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> randomTheta(-3.141592f, 3.141592f);
  std::uniform_real_distribution<float> randomSpeed(1.5f, 2.0f);
  std::uniform_real_distribution<float> randomLife(0.0f, 1.0f);

  int newCount = 10;
  for (auto& p : g_particlesCPU) {
    if (p.life < 0.0f && newCount > 0) {
      const float theta = randomTheta(gen);
      p.position =
          Vector3(cos(theta), -sin(theta), 0.0f) * randomLife(gen) * 0.1f;
      p.velocity = Vector3(-1.0f, 0.0f, 0.0f) * randomSpeed(gen);
      p.life = randomLife(gen) * 1.5f;
      newCount--;
    }
  }

  const Vector3 gravity = Vector3(0.0f, -9.8f, 0.0f);

  for (auto& p : g_particlesCPU) {
    if (p.life < 0.0f) continue;

    p.velocity += gravity * dt;
    p.position += p.velocity * dt;
    p.life -= dt;
  }

  D3D11_MAPPED_SUBRESOURCE ms = {};
  g_context->Map(g_particlesStagingGPU.Get(), 0, D3D11_MAP_WRITE, 0, &ms);
  memcpy(ms.pData, g_particlesCPU.data(), sizeof(Particle) * MAX_PARTICLES);
  g_context->Unmap(g_particlesStagingGPU.Get(), 0);

  g_context->CopyResource(g_particlesGPU.Get(), g_particlesStagingGPU.Get());
}

bool DoTest() {
  const float clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  g_context->ClearRenderTargetView(g_renderTargetView.Get(), clearColor);

  g_context->OMSetRenderTargets(1, g_renderTargetView.GetAddressOf(), nullptr);
  g_context->VSSetShader(g_vertexShader.Get(), nullptr, 0);
  g_context->PSSetShader(g_pixelShader.Get(), nullptr, 0);

  g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
  g_context->VSSetShaderResources(0, 1, g_particlesSRV.GetAddressOf());
  g_context->Draw(static_cast<UINT>(g_particlesCPU.size()), 0);

  g_context->Flush();

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
  g_particlesGPU.Reset();
  g_particlesStagingGPU.Reset();

  // InputLayouts
  g_inputLayout.Reset();

  // Buffers
  g_quadRendererCB.Reset();

  // Shaders
  g_vertexShader.Reset();
  g_pixelShader.Reset();

  // Views
  g_renderTargetView.Reset();
  g_depthStencilView.Reset();
  g_sharedSRV.Reset();
  g_particlesSRV.Reset();
  g_particlesUAV.Reset();

  // Resources
  g_renderTargetBuffer.Reset();
  g_depthStencilBuffer.Reset();

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

      // Create the vertex input layout.
      D3D11_INPUT_ELEMENT_DESC vertexDesc[] = {
          {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
           D3D11_INPUT_PER_VERTEX_DATA, 0},
      };

      // Create the input layout
      hr = g_device->CreateInputLayout(vertexDesc, 1, byteCode.data(),
                                       byteCode.size(),
                                       g_inputLayout.ReleaseAndGetAddressOf());
      if (FAILED(hr)) FailRet("CreateInputLayout Failed.");
    } else if (shaderProfile == "PS") {
      HRESULT hr = g_device->CreatePixelShader(
          byteCode.data(), byteCode.size(), nullptr,
          reinterpret_cast<ID3D11PixelShader**>(deviceChild));
      if (FAILED(hr)) FailRet("CreatePixelShader Failed.");
    }

    return true;
  };

  if (!RegisterShaderObjFile("VS_ParticleSystem", "VS",
                             reinterpret_cast<ID3D11DeviceChild**>(
                                 g_vertexShader.ReleaseAndGetAddressOf())))
    FailRet("RegisterShaderObjFile Failed.");
  if (!RegisterShaderObjFile("PS_ParticleSystem", "PS",
                             reinterpret_cast<ID3D11DeviceChild**>(
                                 g_pixelShader.ReleaseAndGetAddressOf())))
    FailRet("RegisterShaderObjFile Failed.");

  return true;
}
}  // namespace my