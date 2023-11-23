#include "MyEngineAPI.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include "SimpleMath.h"
#include "spdlog/spdlog.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace DirectX::SimpleMath;

struct Vertex {
  Vector3 position;
  XMCOLOR color;
};

struct ObjectConstants {
  Matrix world;
  Matrix view;
  Matrix projection;
};

std::shared_ptr<spdlog::logger> g_apiLogger;

static ComPtr<ID3D11Device> g_device;
static ComPtr<ID3D11DeviceContext> g_context;

static ComPtr<ID3D11Texture2D> g_renderTargetBuffer;
static ComPtr<ID3D11Texture2D> g_depthStencilBuffer;
static ComPtr<ID3D11RenderTargetView> g_renderTargetView;
static ComPtr<ID3D11DepthStencilView> g_depthStencilView;
static ComPtr<ID3D11RasterizerState> g_rasterizerState;
static ComPtr<ID3D11DepthStencilState> g_depthStencilState;
static D3D11_VIEWPORT g_viewport;

static ComPtr<ID3D11Buffer> g_vertexBuffer;
static ComPtr<ID3D11Buffer> g_constantBuffer;
static ComPtr<ID3D11VertexShader> g_vertexShader;
static ComPtr<ID3D11PixelShader> g_pixelShader;
static ComPtr<ID3D11InputLayout> g_inputLayout;

static ObjectConstants g_objConstants;

bool my::InitEngine(spdlog::logger* spdlogPtr) {
  g_apiLogger = std::make_shared<spdlog::logger>(*spdlogPtr);

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

  if (FAILED(hr)) {
    g_apiLogger->error("D3D11CreateDevice Failed.");
    return false;
  }

  if (featureLevel != D3D_FEATURE_LEVEL_11_0) {
    g_apiLogger->error("Direct3D Feature Level 11 unsupported.");
    return false;
  }

  // Create vertex buffer
  Vertex vertices[] = {
      {Vector3(0.0f, 0.5f, 0.5f), XMCOLOR(0.0f, 0.0f, 0.5f, 1.0f)},
      {Vector3(0.5f, -0.5f, 0.5f), XMCOLOR(0.5f, 0.0f, 0.0f, 1.0f)},
      {Vector3(-0.5f, -0.5f, 0.5f), XMCOLOR(0.0f, 0.5f, 0.0f, 1.0f)},
  };

  D3D11_BUFFER_DESC vertexBufferDesc = {};
  vertexBufferDesc.ByteWidth = sizeof(Vertex) * 3;
  vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
  vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  vertexBufferDesc.CPUAccessFlags = 0;
  vertexBufferDesc.MiscFlags = 0;

  D3D11_SUBRESOURCE_DATA vertexBufferData = {};
  vertexBufferData.pSysMem = vertices;
  vertexBufferData.SysMemPitch = 0;
  vertexBufferData.SysMemSlicePitch = 0;

  hr = g_device->CreateBuffer(&vertexBufferDesc, &vertexBufferData,
                              g_vertexBuffer.ReleaseAndGetAddressOf());

  if (FAILED(hr)) {
    g_apiLogger->error("CreateBuffer Failed.");
    return false;
  }

  D3D11_BUFFER_DESC constantBufferDesc = {};
  constantBufferDesc.ByteWidth = sizeof(ObjectConstants);
  constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
  constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  constantBufferDesc.MiscFlags = 0;

  D3D11_SUBRESOURCE_DATA constantBufferData = {};
  constantBufferData.pSysMem = &g_objConstants;
  constantBufferData.SysMemPitch = 0;
  constantBufferData.SysMemSlicePitch = 0;

  hr = g_device->CreateBuffer(&constantBufferDesc, &constantBufferData,
                              g_constantBuffer.ReleaseAndGetAddressOf());

  if (FAILED(hr)) {
    g_apiLogger->error("CreateBuffer Failed.");
    return false;
  }

  UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
  compileFlags |= D3DCOMPILE_DEBUG;
#endif

  ComPtr<ID3DBlob> byteCode;
  ComPtr<ID3DBlob> errors;

  // Compile vertex shader
  hr = D3DCompileFromFile(L"../MyEngine/hlsl/color_vs.hlsl", nullptr, nullptr,
                          "main", "vs_5_0", compileFlags, 0,
                          byteCode.ReleaseAndGetAddressOf(),
                          errors.ReleaseAndGetAddressOf());

  if (errors != nullptr) g_apiLogger->error((char*)errors->GetBufferPointer());

  if (FAILED(hr)) {
    g_apiLogger->error("D3DCompileFromFile Failed.");
    return false;
  }

  hr = g_device->CreateVertexShader(byteCode->GetBufferPointer(),
                                    byteCode->GetBufferSize(), nullptr,
                                    g_vertexShader.ReleaseAndGetAddressOf());

  if (FAILED(hr)) {
    g_apiLogger->error("CreateVertexShader Failed.");
    return false;
  }

  // Create the vertex input layout.
  D3D11_INPUT_ELEMENT_DESC vertexDesc[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
       D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 12,
       D3D11_INPUT_PER_VERTEX_DATA, 0},
  };

  // Create the input layout
  hr = g_device->CreateInputLayout(vertexDesc, 2, byteCode->GetBufferPointer(),
                                   byteCode->GetBufferSize(),
                                   g_inputLayout.ReleaseAndGetAddressOf());

  if (FAILED(hr)) {
    g_apiLogger->error("CreateInputLayout Failed.");
    return false;
  }

  // Compile pixel shader
  hr = D3DCompileFromFile(L"../MyEngine/hlsl/color_ps.hlsl", nullptr, nullptr,
                          "main", "ps_5_0", compileFlags, 0,
                          byteCode.ReleaseAndGetAddressOf(),
                          errors.ReleaseAndGetAddressOf());

  if (errors != nullptr) g_apiLogger->error((char*)errors->GetBufferPointer());

  if (FAILED(hr)) {
    g_apiLogger->error("D3DCompileFromFile Failed.");
    return false;
  }

  hr = g_device->CreatePixelShader(byteCode->GetBufferPointer(),
                                   byteCode->GetBufferSize(), nullptr,
                                   g_pixelShader.ReleaseAndGetAddressOf());

  if (FAILED(hr)) {
    g_apiLogger->error("CreatePixelShader Failed.");
    return false;
  }

  byteCode.Reset();
  errors.Reset();

  g_objConstants.world = Matrix().Transpose();

  // Build the view matrix.
  Vector3 pos(0.0f, 0.0f, -1.0f);
  Vector3 forward(0.0f, 0.0f, 1.0f);
  Vector3 up(0.0f, 1.0f, 0.0f);

  g_objConstants.view =
      Matrix::CreateLookAt(pos, pos + forward, up).Transpose();

  return true;
}

bool my::SetRenderTargetSize(int w, int h) {
  HRESULT hr = S_OK;

  D3D11_TEXTURE2D_DESC renderTargetBufferDesc = {};

  renderTargetBufferDesc.Width = w;
  renderTargetBufferDesc.Height = h;
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

  hr = g_device->CreateTexture2D(&renderTargetBufferDesc, nullptr,
                                 g_renderTargetBuffer.ReleaseAndGetAddressOf());

  if (FAILED(hr)) {
    g_apiLogger->error("CreateTexture2D Failed.");
    return false;
  }

  D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc = {};

  renderTargetViewDesc.Format = renderTargetBufferDesc.Format;
  renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
  renderTargetViewDesc.Texture2D.MipSlice = 0;

  hr = g_device->CreateRenderTargetView(
      g_renderTargetBuffer.Get(), &renderTargetViewDesc,
      g_renderTargetView.ReleaseAndGetAddressOf());

  if (FAILED(hr)) {
    g_apiLogger->error("CreateRenderTargetView Failed.");
    return false;
  }

  // Create the depth/stencil buffer and view.

  D3D11_TEXTURE2D_DESC depthStencilBufferDesc = {};

  depthStencilBufferDesc.Width = w;
  depthStencilBufferDesc.Height = h;
  depthStencilBufferDesc.MipLevels = 1;
  depthStencilBufferDesc.ArraySize = 1;
  depthStencilBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  depthStencilBufferDesc.SampleDesc.Count = 1;
  depthStencilBufferDesc.SampleDesc.Quality = 0;
  depthStencilBufferDesc.Usage = D3D11_USAGE_DEFAULT;
  depthStencilBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
  depthStencilBufferDesc.CPUAccessFlags = 0;
  depthStencilBufferDesc.MiscFlags = 0;

  hr = (g_device->CreateTexture2D(
      &depthStencilBufferDesc, nullptr,
      g_depthStencilBuffer.ReleaseAndGetAddressOf()));

  if (FAILED(hr)) {
    g_apiLogger->error("CreateTexture2D Failed.");
    return false;
  }

  hr = (g_device->CreateDepthStencilView(
      g_depthStencilBuffer.Get(), nullptr,
      g_depthStencilView.ReleaseAndGetAddressOf()));

  if (FAILED(hr)) {
    g_apiLogger->error("CreateDepthStencilView Failed.");
    return false;
  }

  D3D11_RASTERIZER_DESC rasterizerDesc = {};
  rasterizerDesc.FillMode = D3D11_FILL_SOLID;
  rasterizerDesc.CullMode = D3D11_CULL_NONE;
  rasterizerDesc.FrontCounterClockwise = false;
  rasterizerDesc.DepthClipEnable = true;

  hr = g_device->CreateRasterizerState(
      &rasterizerDesc, g_rasterizerState.ReleaseAndGetAddressOf());

  if (FAILED(hr)) {
    g_apiLogger->error("CreateRasterizerState Failed.");
    return false;
  }

  D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
  depthStencilDesc.DepthEnable = true;
  depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
  depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

  hr = g_device->CreateDepthStencilState(
      &depthStencilDesc, g_depthStencilState.ReleaseAndGetAddressOf());

  if (FAILED(hr)) {
    g_apiLogger->error("CreateDepthStencilState Failed.");
    return false;
  }

  // Bind the render target view and depth/stencil view to the pipeline.

  g_context->OMSetRenderTargets(1, g_renderTargetView.GetAddressOf(),
                                g_depthStencilView.Get());

  // Set the viewport transform.

  g_viewport.TopLeftX = 0.0f;
  g_viewport.TopLeftY = 0.0f;
  g_viewport.Width = static_cast<float>(w);
  g_viewport.Height = static_cast<float>(h);
  g_viewport.MinDepth = 0.0f;
  g_viewport.MaxDepth = 1.0f;

  g_context->RSSetViewports(1, &g_viewport);

  // The window resized, so update the aspect ratio and recompute the projection
  // matrix.
  g_objConstants.projection =
      Matrix::CreatePerspectiveFieldOfView(
          0.25f * XM_PI, static_cast<float>(w) / h, 1.0f, 1000.0f)
          .Transpose();

  return true;
}

bool my::DoTest(Vector2 mouseDragDeltaLeft, Vector2 mouseDragDeltaRight) {
  HRESULT hr = S_OK;

  D3D11_MAPPED_SUBRESOURCE mappedResource;
  ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
  g_context->Map(g_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
                 &mappedResource);
  memcpy(mappedResource.pData, &g_objConstants, sizeof(g_objConstants));
  g_context->Unmap(g_constantBuffer.Get(), 0);

  float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
  g_context->ClearRenderTargetView(g_renderTargetView.Get(), clearColor);
  g_context->ClearDepthStencilView(g_depthStencilView.Get(),
                                   D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
                                   1.0f, 0);

  UINT stride = sizeof(Vertex);
  UINT offset = 0;
  g_context->IASetInputLayout(g_inputLayout.Get());
  g_context->IASetVertexBuffers(0, 1, g_vertexBuffer.GetAddressOf(), &stride,
                                &offset);
  g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  g_context->VSSetShader(g_vertexShader.Get(), nullptr, 0);
  g_context->VSSetConstantBuffers(0, 1, g_constantBuffer.GetAddressOf());
  g_context->PSSetShader(g_pixelShader.Get(), nullptr, 0);

  g_context->OMSetDepthStencilState(g_depthStencilState.Get(), 1);
  g_context->RSSetState(g_rasterizerState.Get());

  g_context->Draw(3, 0);

  g_context->Flush();

  return true;
}

bool my::GetRenderTarget(ID3D11Device* device,
                         ID3D11ShaderResourceView** textureView) {
  HRESULT hr = S_OK;

  ComPtr<IDXGIResource> dxgiResource;

  hr = g_renderTargetBuffer->QueryInterface(
      __uuidof(IDXGIResource),
      reinterpret_cast<void**>(dxgiResource.ReleaseAndGetAddressOf()));

  if (FAILED(hr)) {
    g_apiLogger->error("QueryInterface Failed.");
    return false;
  }

  HANDLE sharedHandle;

  hr = dxgiResource->GetSharedHandle(&sharedHandle);
  dxgiResource.Reset();

  if (FAILED(hr)) {
    g_apiLogger->error("GetSharedHandle Failed.");
    return false;
  }

  ComPtr<ID3D11Texture2D> texture;

  hr = device->OpenSharedResource(
      sharedHandle, __uuidof(ID3D11Texture2D),
      reinterpret_cast<void**>(texture.ReleaseAndGetAddressOf()));

  if (FAILED(hr)) {
    g_apiLogger->error("OpenSharedResource Failed.");
    return false;
  }

  D3D11_TEXTURE2D_DESC desc = {};
  texture->GetDesc(&desc);

  // Create texture view
  D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
  ZeroMemory(&SRVDesc, sizeof(SRVDesc));
  SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  SRVDesc.Texture2D.MipLevels = desc.MipLevels;
  SRVDesc.Texture2D.MostDetailedMip = 0;

  device->CreateShaderResourceView(texture.Get(), &SRVDesc, textureView);

  if (FAILED(hr)) {
    g_apiLogger->error("CreateShaderResourceView Failed.");
    return false;
  }

  texture.Reset();

  return true;
}

void my::DeinitEngine() {
  g_vertexBuffer.Reset();
  g_constantBuffer.Reset();
  g_vertexShader.Reset();
  g_pixelShader.Reset();

  g_inputLayout.Reset();

  g_rasterizerState.Reset();
  g_depthStencilState.Reset();

  g_renderTargetView.Reset();
  g_depthStencilView.Reset();
  g_renderTargetBuffer.Reset();
  g_depthStencilBuffer.Reset();

  g_context.Reset();
  g_device.Reset();
}