#include "MyEngineAPI.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX::SimpleMath;

struct PostRenderer {
  Vector3 posCam;  // WS
  int lightColor;

  Vector3 posLight;  // WS
  float lightIntensity;

  Matrix matPS2WS;

  Vector2 rtSize;
  Vector2 dummy0;

  Vector3 distBoxCenter;  // WS
  float distBoxSize;      // WS
};

std::shared_ptr<spdlog::logger> g_apiLogger;

ComPtr<ID3D11Device> g_device;
ComPtr<ID3D11DeviceContext> g_context;

// Resources
ComPtr<ID3D11Texture2D> g_renderTargetBuffer;
ComPtr<ID3D11Texture2D> g_depthStencilBuffer;

// Views
ComPtr<ID3D11RenderTargetView> g_renderTargetView;
ComPtr<ID3D11DepthStencilView> g_depthStencilView;
ComPtr<ID3D11ShaderResourceView> g_sharedSRV;

// Shaders
ComPtr<ID3D11VertexShader> g_vertexShader;
ComPtr<ID3D11PixelShader> g_pixelShader;

// Buffers
ComPtr<ID3D11Buffer> g_screenQuadVB;
ComPtr<ID3D11Buffer> g_constantBuffer;

// States
ComPtr<ID3D11RasterizerState> g_rasterizerState;
ComPtr<ID3D11DepthStencilState> g_depthStencilState;

// InputLayouts
ComPtr<ID3D11InputLayout> g_inputLayout;

Vector3 g_posLight;
int g_lightColor;
float g_lightIntensity;

Matrix g_view;
Matrix g_proj;

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

  UINT createDeviceFlags = 0;
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
                              g_constantBuffer.ReleaseAndGetAddressOf());

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

  g_view = Matrix::Identity;
  g_proj = Matrix::Identity;

  g_posLight = Vector3(0.0f, 1.0f, 0.0f);
  g_lightColor = 0xffffffff;
  g_lightIntensity = 1.0f;

  g_distBoxCenter = Vector3(0.0f, 0.0f, 0.0f);
  g_distBoxSize = 2.0f;

  return true;
}

bool my::SetRenderTargetSize(int w, int h) {
  g_renderTargetWidth = w;
  g_renderTargetHeight = h;

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

  return true;
}

bool my::DoTest() {
  PostRenderer quadPostRenderer = {};
  quadPostRenderer.posCam = g_view.Translation();
  quadPostRenderer.lightColor = g_lightColor;
  quadPostRenderer.posLight = g_posLight;
  quadPostRenderer.lightIntensity = g_lightIntensity;
  quadPostRenderer.matPS2WS = (g_view * g_proj).Invert();
  quadPostRenderer.rtSize = Vector2(g_renderTargetWidth, g_renderTargetHeight);
  quadPostRenderer.distBoxCenter = g_distBoxCenter;
  quadPostRenderer.distBoxSize = g_distBoxSize;

  D3D11_MAPPED_SUBRESOURCE mappedResource = {};
  g_context->Map(g_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
                 &mappedResource);
  memcpy(mappedResource.pData, &quadPostRenderer, sizeof(quadPostRenderer));
  g_context->Unmap(g_constantBuffer.Get(), 0);

  UINT stride = sizeof(Vector3);
  UINT offset = 0;
  g_context->IASetInputLayout(g_inputLayout.Get());
  g_context->IASetVertexBuffers(0, 1, g_screenQuadVB.GetAddressOf(), &stride,
                                &offset);
  g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

  g_context->VSSetShader(g_vertexShader.Get(), nullptr, 0);
  g_context->PSSetShader(g_pixelShader.Get(), nullptr, 0);
  g_context->PSSetConstantBuffers(0, 1, g_constantBuffer.GetAddressOf());

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
  D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
  ZeroMemory(&SRVDesc, sizeof(SRVDesc));
  SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  SRVDesc.Texture2D.MipLevels = desc.MipLevels;
  SRVDesc.Texture2D.MostDetailedMip = 0;

  dx11ImGuiDevice->CreateShaderResourceView(texture.Get(), &SRVDesc, sharedSRV);

  if (FAILED(hr)) {
    g_apiLogger->error("CreateShaderResourceView Failed.");
    return false;
  }

  texture.Reset();

  return true;
}

void my::DeinitEngine() {
  // States
  g_rasterizerState.Reset();
  g_depthStencilState.Reset();

  // InputLayouts
  g_inputLayout.Reset();

  // Buffers
  g_screenQuadVB.Reset();
  g_constantBuffer.Reset();

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
                                 g_vertexShader.ReleaseAndGetAddressOf())))
    FailRet("RegisterShaderObjFile Failed.");
  if (!RegisterShaderObjFile("PS_RayMARCH", "PS",
                             reinterpret_cast<ID3D11DeviceChild**>(
                                 g_pixelShader.ReleaseAndGetAddressOf())))
    FailRet("RegisterShaderObjFile Failed.");

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