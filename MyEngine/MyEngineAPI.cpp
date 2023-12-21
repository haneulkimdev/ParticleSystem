#include "MyEngineAPI.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::SimpleMath;

using float4x4 = Matrix;
using float2 = Vector2;
using float3 = Vector3;
using uint = uint32_t;

struct Particle {
  float3 position = Vector3(0.0f);
  float mass = 1.0f;
  float3 force = Vector3(0.0f);
  float rotationalVelocity = 0.0f;
  float3 velocity = Vector3(0.0f);
  float maxLife = 1.0f;
  float2 sizeBeginEnd = Vector2(1.0f);
  float life = maxLife;
  uint color = my::ColorConvertFloat4ToU32(Color(1.0f, 1.0f, 1.0f, 1.0f));
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

  float4x4 matPS2WS;

  float2 rtSize;
  float smoothingCoefficient;
  float dummy0;

  float3 distBoxCenter;  // WS
  float distBoxSize;     // WS
};

std::shared_ptr<spdlog::logger> g_apiLogger;

ComPtr<ID3D11Device> g_device;
ComPtr<ID3D11DeviceContext> g_context;

// Resources
ComPtr<ID3D11Texture2D> g_renderTargetBuffer;
ComPtr<ID3D11Texture2D> g_depthStencilBuffer;

// Views
ComPtr<ID3D11ShaderResourceView> g_particleSRV;
ComPtr<ID3D11RenderTargetView> g_renderTargetView;
ComPtr<ID3D11DepthStencilView> g_depthStencilView;
ComPtr<ID3D11ShaderResourceView> g_sharedSRV;

// Shaders
ComPtr<ID3D11VertexShader> g_quadVS;
ComPtr<ID3D11PixelShader> g_rayMarchPS;

// Buffers
ComPtr<ID3D11Buffer> g_particleBuffer;
ComPtr<ID3D11Buffer> g_screenQuadVB;
ComPtr<ID3D11Buffer> g_quadRendererCB;

// States
ComPtr<ID3D11RasterizerState> g_rasterizerState;
ComPtr<ID3D11DepthStencilState> g_depthStencilState;

// InputLayouts
ComPtr<ID3D11InputLayout> g_inputLayout;

const uint32_t MAX_PARTICLES = 4;

Particle g_particles[MAX_PARTICLES];

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

bool my::InitEngine(std::shared_ptr<spdlog::logger> spdlogPtr) {
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

  D3D11_BUFFER_DESC particleBufferDesc = {};
  particleBufferDesc.ByteWidth = sizeof(Particle) * MAX_PARTICLES;
  particleBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
  particleBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  particleBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  particleBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
  particleBufferDesc.StructureByteStride = sizeof(Particle);

  hr = g_device->CreateBuffer(&particleBufferDesc, nullptr,
                              g_particleBuffer.ReleaseAndGetAddressOf());

  if (FAILED(hr)) FailRet("CreateBuffer Failed.");

  D3D11_SHADER_RESOURCE_VIEW_DESC particleSRVDesc = {};
  particleSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
  particleSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
  particleSRVDesc.Buffer.FirstElement = 0;
  particleSRVDesc.Buffer.ElementWidth = MAX_PARTICLES;

  hr = g_device->CreateShaderResourceView(
      g_particleBuffer.Get(), &particleSRVDesc,
      g_particleSRV.ReleaseAndGetAddressOf());

  if (FAILED(hr)) FailRet("CreateShaderResourceView Failed.");

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

  my::BuildScreenQuadGeometryBuffers();

  my::LoadShaders();

  my::SetParticleSize(0, 1.0f);
  my::SetParticleSize(1, 0.4f);
  my::SetParticleSize(2, 0.7f);
  my::SetParticleSize(3, 0.4f);

  my::SetParticleColor(0, Color(0.0f, 1.0f, 0.0f, 1.0f));
  my::SetParticleColor(1, Color(0.0f, 0.5f, 1.0f, 1.0f));
  my::SetParticleColor(2, Color(1.0f, 0.2f, 0.0f, 1.0f));
  my::SetParticleColor(3, Color(1.0f, 1.0f, 0.0f, 1.0f));

  // Build the view matrix.
  Vector3 pos(0.0f, 0.0f, -5.0f);
  Vector3 forward(0.0f, 0.0f, 1.0f);
  Vector3 up(0.0f, 1.0f, 0.0f);

  g_camera.LookAt(pos, pos + forward, up);
  g_camera.UpdateViewMatrix();

  g_pointLight.position = g_camera.GetPosition();
  g_pointLight.color = ColorConvertFloat4ToU32(Color(1.0f, 1.0f, 1.0f, 1.0f));
  g_pointLight.intensity = 1.0f;

  g_distBoxCenter = Vector3(0.0f, 0.0f, 0.0f);
  g_distBoxSize = 2.0f;

  return true;
}

bool my::SetRenderTargetSize(int w, int h) {
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

  g_context->OMSetRenderTargets(1, g_renderTargetView.GetAddressOf(), nullptr);

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

uint32_t my::GetMaxParticleCount() { return MAX_PARTICLES; }

Vector3 my::GetParticlePosition(int index) {
  return g_particles[index].position;
}

float my::GetParticleSize(int index) {
  return g_particles[index].sizeBeginEnd.x;
}

Color my::GetParticleColor(int index) {
  return my::ColorConvertU32ToFloat4(g_particles[index].color);
}

Vector3 my::GetLightPosition() { return g_pointLight.position; }

float my::GetLightIntensity() { return g_pointLight.intensity; }

Color my::GetLightColor() {
  return my::ColorConvertU32ToFloat4(g_pointLight.color);
}

Vector3 my::GetDistBoxCenter() { return g_distBoxCenter; }

float my::GetDistBoxSize() { return g_distBoxSize; }

float my::GetSmoothingCoefficient() { return g_smoothingCoefficient; }

void my::SetParticlePosition(int index, const Vector3& position) {
  g_particles[index].position = position;
}

void my::SetParticleSize(int index, float size) {
  g_particles[index].sizeBeginEnd = Vector2(size);
}

void my::SetParticleColor(int index, const Color& color) {
  g_particles[index].color = my::ColorConvertFloat4ToU32(color);
}

void my::SetLightPosition(const Vector3& position) {
  g_pointLight.position = position;
}

void my::SetLightIntensity(float intensity) {
  g_pointLight.intensity = intensity;
}

void my::SetLightColor(const Color& color) {
  g_pointLight.color = my::ColorConvertFloat4ToU32(color);
}

void my::SetDistBoxCenter(const Vector3& center) { g_distBoxCenter = center; }

void my::SetDistBoxSize(float size) { g_distBoxSize = size; }

void my::SetSmoothingCoefficient(float smoothingCoefficient) {
  g_smoothingCoefficient = smoothingCoefficient;
}

void my::UpdateParticleBuffer() {
  D3D11_MAPPED_SUBRESOURCE mappedResource = {};
  g_context->Map(g_particleBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
                 &mappedResource);
  memcpy(mappedResource.pData, g_particles, sizeof(g_particles));
  g_context->Unmap(g_particleBuffer.Get(), 0);
}

bool my::DoTest() {
  PostRenderer quadPostRenderer = {};
  quadPostRenderer.posCam = g_camera.GetPosition();
  quadPostRenderer.lightColor = g_pointLight.color;
  quadPostRenderer.posLight = g_pointLight.position;
  quadPostRenderer.lightIntensity = g_pointLight.intensity;
  quadPostRenderer.matPS2WS = g_camera.ViewProj().Invert().Transpose();
  quadPostRenderer.rtSize = Vector2(static_cast<float>(g_renderTargetWidth),
                                    static_cast<float>(g_renderTargetHeight));
  quadPostRenderer.smoothingCoefficient = g_smoothingCoefficient;
  quadPostRenderer.distBoxCenter = g_distBoxCenter;
  quadPostRenderer.distBoxSize = g_distBoxSize;

  D3D11_MAPPED_SUBRESOURCE mappedResource = {};
  g_context->Map(g_quadRendererCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
                 &mappedResource);
  memcpy(mappedResource.pData, &quadPostRenderer, sizeof(quadPostRenderer));
  g_context->Unmap(g_quadRendererCB.Get(), 0);

  uint32_t stride = sizeof(Vector3);
  uint32_t offset = 0;
  g_context->IASetInputLayout(g_inputLayout.Get());
  g_context->IASetVertexBuffers(0, 1, g_screenQuadVB.GetAddressOf(), &stride,
                                &offset);
  g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

  g_context->VSSetShader(g_quadVS.Get(), nullptr, 0);
  g_context->PSSetShader(g_rayMarchPS.Get(), nullptr, 0);
  g_context->PSSetShaderResources(0, 1, g_particleSRV.GetAddressOf());
  g_context->PSSetConstantBuffers(0, 1, g_quadRendererCB.GetAddressOf());

  g_context->OMSetDepthStencilState(g_depthStencilState.Get(), 1);
  g_context->RSSetState(g_rasterizerState.Get());

  g_context->Draw(4, 0);

  g_context->Flush();

  return true;
}

bool my::GetDX11SharedRenderTarget(ID3D11Device* dx11ImGuiDevice,
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

void my::DeinitEngine() {
  // States
  g_rasterizerState.Reset();
  g_depthStencilState.Reset();

  // InputLayouts
  g_inputLayout.Reset();

  // Buffers
  g_particleBuffer.Reset();
  g_screenQuadVB.Reset();
  g_quadRendererCB.Reset();

  // Shaders
  g_quadVS.Reset();
  g_rayMarchPS.Reset();

  // Views
  g_particleSRV.Reset();
  g_renderTargetView.Reset();
  g_depthStencilView.Reset();
  g_sharedSRV.Reset();

  // Resources
  g_renderTargetBuffer.Reset();
  g_depthStencilBuffer.Reset();

  g_context.Reset();
  g_device.Reset();
}

bool my::LoadShaders() {
  std::string enginePath;
  if (!GetEnginePath(enginePath))
    return FailRet("Failure to Read Engine Path.");

  auto RegisterShaderObjFile = [&enginePath](
                                   const std::string& shaderObjFileName,
                                   const std::string& shaderProfile,
                                   ID3D11DeviceChild** deviceChild) -> bool {
    std::vector<BYTE> byteCode;
    my::ReadData(enginePath + "/hlsl/obj/" + shaderObjFileName, byteCode);

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

  if (!RegisterShaderObjFile("VS_QUAD", "VS",
                             reinterpret_cast<ID3D11DeviceChild**>(
                                 g_quadVS.ReleaseAndGetAddressOf())))
    FailRet("RegisterShaderObjFile Failed.");
  if (!RegisterShaderObjFile("PS_RayMARCH", "PS",
                             reinterpret_cast<ID3D11DeviceChild**>(
                                 g_rayMarchPS.ReleaseAndGetAddressOf())))
    FailRet("RegisterShaderObjFile Failed.");

  return true;
}

bool my::GetDevice(ID3D11Device* device) {
  device = nullptr;

  if (!g_device) return FailRet("Device not initialized.");

  device = g_device.Get();

  return true;
}

bool my::GetEnginePath(std::string& enginePath) {
  char ownPth[2048];
  GetModuleFileNameA(nullptr, ownPth, sizeof(ownPth));
  std::string exe_path = ownPth;
  std::string exe_path_;
  size_t pos = 0;
  std::string token;
  std::string delimiter = "\\";
  while ((pos = exe_path.find(delimiter)) != std::string::npos) {
    token = exe_path.substr(0, pos);
    if (token.find(".exe") != std::string::npos) break;
    exe_path += token + "\\";
    exe_path_ += token + "\\";
    exe_path.erase(0, pos + delimiter.length());
  }

  std::ifstream file(exe_path_ + "..\\engine_module_path.txt");

  if (!file.is_open()) return FailRet("engine_module_path.txt not found.");

  getline(file, enginePath);

  file.close();

  return true;
}

bool my::ReadData(const std::string& name, std::vector<BYTE>& blob) {
  std::ifstream fin(name, std::ios::binary);

  if (!fin) FailRet("File not found.");

  blob.assign((std::istreambuf_iterator<char>(fin)),
              std::istreambuf_iterator<char>());

  fin.close();

  return true;
}

bool my::BuildScreenQuadGeometryBuffers() {
  Vector3 vertices[4] = {
      Vector3(-1.0f, 1.0f, 0.0f),
      Vector3(1.0f, 1.0f, 0.0f),
      Vector3(-1.0f, -1.0f, 0.0f),
      Vector3(1.0f, -1.0f, 0.0f),
  };

  D3D11_BUFFER_DESC vertexBufferDesc = {};
  vertexBufferDesc.ByteWidth = sizeof(Vector3) * 4;
  vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
  vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  vertexBufferDesc.CPUAccessFlags = 0;
  vertexBufferDesc.MiscFlags = 0;

  D3D11_SUBRESOURCE_DATA vertexBufferData = {};
  vertexBufferData.pSysMem = vertices;
  vertexBufferData.SysMemPitch = 0;
  vertexBufferData.SysMemSlicePitch = 0;

  HRESULT hr = g_device->CreateBuffer(&vertexBufferDesc, &vertexBufferData,
                                      g_screenQuadVB.ReleaseAndGetAddressOf());

  if (FAILED(hr)) FailRet("CreateBuffer Failed.");

  return true;
}

Color my::ColorConvertU32ToFloat4(uint32_t color) {
  float s = 1.0f / 255.0f;
  return Vector4(((color)&0xFF) * s, ((color >> 8) & 0xFF) * s,
                 ((color >> 16) & 0xFF) * s, ((color >> 24) & 0xFF) * s);
}

uint32_t my::ColorConvertFloat4ToU32(const Color& color) {
  return static_cast<uint32_t>(color.R() * 255.0f + 0.5f) |
         (static_cast<uint32_t>(color.G() * 255.0f + 0.5f) << 8) |
         (static_cast<uint32_t>(color.B() * 255.0f + 0.5f) << 16) |
         (static_cast<uint32_t>(color.A() * 255.0f + 0.5f) << 24);
}