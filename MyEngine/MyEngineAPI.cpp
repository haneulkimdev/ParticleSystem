#include "MyEngineAPI.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace DirectX::SimpleMath;

struct Vertex {
  Vector3 pos;
  XMCOLOR color;
  Vector3 normal;
  Vector2 tex;
};

struct ObjectConstants {
  Matrix world;
  Matrix worldInvTranspose;
  Matrix view;
  Matrix proj;
};

std::shared_ptr<spdlog::logger> g_apiLogger;

static ComPtr<ID3D11Device> g_device;
static ComPtr<ID3D11DeviceContext> g_context;

static ComPtr<ID3D11VertexShader> g_geometryVS;
static ComPtr<ID3D11PixelShader> g_geometryPS;

static ComPtr<ID3D11Buffer> g_screenQuadVB;
static ComPtr<ID3D11Buffer> g_screenQuadIB;

static ComPtr<ID3D11RenderTargetView> g_posColorRTV;
static ComPtr<ID3D11ShaderResourceView> g_posColorSRV;

static ComPtr<ID3D11RenderTargetView> g_normalRTV;
static ComPtr<ID3D11ShaderResourceView> g_normalSRV;

static ComPtr<ID3D11Texture2D> g_renderTargetBuffer;
static ComPtr<ID3D11Texture2D> g_depthStencilBuffer;
static ComPtr<ID3D11RenderTargetView> g_renderTargetView;
static ComPtr<ID3D11DepthStencilView> g_depthStencilView;
static ComPtr<ID3D11RasterizerState> g_rasterizerState;
static ComPtr<ID3D11DepthStencilState> g_depthStencilState;
static D3D11_VIEWPORT g_viewport;

static ComPtr<ID3D11Buffer> g_vertexBuffer;
static ComPtr<ID3D11Buffer> g_indexBuffer;
static ComPtr<ID3D11Buffer> g_constantBuffer;
static ComPtr<ID3D11VertexShader> g_lightVS;
static ComPtr<ID3D11PixelShader> g_lightPS;
static ComPtr<ID3D11InputLayout> g_inputLayout;

// Define transformations from local spaces to world space.
static Matrix g_sphereWorld;

static UINT g_sphereIndexCount;

static Camera g_cam;

static int g_renderTargetWidth;
static int g_renderTargetHeight;

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

  GeometryGenerator geoGen;
  GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);

  g_sphereIndexCount = sphere.indices.size();

  std::vector<Vertex> vertices(sphere.vertices.size());
  for (size_t i = 0; i < sphere.vertices.size(); i++) {
    vertices[i].pos = sphere.vertices[i].position;

    float r = (sphere.vertices[i].normal.x + 1.0f) * 0.5f;
    float g = (sphere.vertices[i].normal.y + 1.0f) * 0.5f;
    float b = (sphere.vertices[i].normal.z + 1.0f) * 0.5f;

    vertices[i].color = XMCOLOR(r, g, b, 1.0f);
    vertices[i].normal = sphere.vertices[i].normal;
    vertices[i].tex = sphere.vertices[i].texC;
  }

  D3D11_BUFFER_DESC vertexBufferDesc = {};
  vertexBufferDesc.ByteWidth = sizeof(Vertex) * vertices.size();
  vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
  vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  vertexBufferDesc.CPUAccessFlags = 0;
  vertexBufferDesc.MiscFlags = 0;

  D3D11_SUBRESOURCE_DATA vertexBufferData = {};
  vertexBufferData.pSysMem = &vertices[0];
  vertexBufferData.SysMemPitch = 0;
  vertexBufferData.SysMemSlicePitch = 0;

  hr = g_device->CreateBuffer(&vertexBufferDesc, &vertexBufferData,
                              g_vertexBuffer.ReleaseAndGetAddressOf());

  if (FAILED(hr)) {
    g_apiLogger->error("CreateBuffer Failed.");
    return false;
  }

  D3D11_BUFFER_DESC indexBufferDesc = {};
  indexBufferDesc.ByteWidth = sizeof(UINT) * g_sphereIndexCount;
  indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
  indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
  indexBufferDesc.CPUAccessFlags = 0;
  indexBufferDesc.MiscFlags = 0;

  D3D11_SUBRESOURCE_DATA indexBufferData = {};
  indexBufferData.pSysMem = &sphere.indices[0];
  indexBufferData.SysMemPitch = 0;
  indexBufferData.SysMemSlicePitch = 0;

  hr = g_device->CreateBuffer(&indexBufferDesc, &indexBufferData,
                              g_indexBuffer.ReleaseAndGetAddressOf());

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

  hr = g_device->CreateBuffer(&constantBufferDesc, nullptr,
                              g_constantBuffer.ReleaseAndGetAddressOf());

  if (FAILED(hr)) {
    g_apiLogger->error("CreateBuffer Failed.");
    return false;
  }

  my::LoadShaders();

  my::BuildScreenQuadGeometryBuffers();

  // Build the view matrix.
  Vector3 pos(0.0f, 0.0f, -5.0f);
  Vector3 forward(0.0f, 0.0f, 1.0f);
  Vector3 up(0.0f, 1.0f, 0.0f);

  g_cam.LookAt(pos, pos + forward, up);

  return true;
}

bool my::SetRenderTargetSize(int w, int h) {
  g_renderTargetWidth = w;
  g_renderTargetHeight = h;

  HRESULT hr = S_OK;

  D3D11_TEXTURE2D_DESC texDesc = {};
  texDesc.Width = g_renderTargetWidth;
  texDesc.Height = g_renderTargetHeight;
  texDesc.MipLevels = 1;
  texDesc.ArraySize = 1;
  texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  texDesc.SampleDesc.Count = 1;
  texDesc.SampleDesc.Quality = 0;
  texDesc.Usage = D3D11_USAGE_DEFAULT;
  texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
  texDesc.CPUAccessFlags = 0;
  texDesc.MiscFlags = 0;

  ComPtr<ID3D11Texture2D> posColorTex;
  hr = g_device->CreateTexture2D(&texDesc, 0,
                                 posColorTex.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    g_apiLogger->error("CreateTexture2D Failed.");
    return false;
  }
  hr = g_device->CreateRenderTargetView(posColorTex.Get(), 0,
                                        g_posColorRTV.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    g_apiLogger->error("CreateRenderTargetView Failed.");
    return false;
  }
  hr = g_device->CreateShaderResourceView(
      posColorTex.Get(), 0, g_posColorSRV.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    g_apiLogger->error("CreateShaderResourceView Failed.");
    return false;
  }

  // view saves a reference.
  posColorTex.Reset();

  ComPtr<ID3D11Texture2D> normalTex;
  hr = g_device->CreateTexture2D(&texDesc, 0,
                                 normalTex.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    g_apiLogger->error("CreateTexture2D Failed.");
    return false;
  }
  hr = g_device->CreateRenderTargetView(normalTex.Get(), 0,
                                        g_normalRTV.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    g_apiLogger->error("CreateRenderTargetView Failed.");
    return false;
  }
  hr = g_device->CreateShaderResourceView(normalTex.Get(), 0,
                                          g_normalSRV.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    g_apiLogger->error("CreateShaderResourceView Failed.");
    return false;
  }

  // view saves a reference.
  normalTex.Reset();

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

  depthStencilBufferDesc.Width = g_renderTargetWidth;
  depthStencilBufferDesc.Height = g_renderTargetHeight;
  depthStencilBufferDesc.MipLevels = 1;
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
  depthStencilDesc.StencilEnable = false;

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
  g_viewport.Width = static_cast<float>(g_renderTargetWidth);
  g_viewport.Height = static_cast<float>(g_renderTargetHeight);
  g_viewport.MinDepth = 0.0f;
  g_viewport.MaxDepth = 1.0f;

  g_context->RSSetViewports(1, &g_viewport);

  // The window resized, so update the aspect ratio and recompute the projection
  // matrix.
  g_cam.SetLens(0.25f * XM_PI,
                static_cast<float>(g_renderTargetWidth) / g_renderTargetHeight,
                1.0f, 1000.0f);

  return true;
}

void my::UpdateScene(int mouseButton, Vector2 lastMousePos, Vector2 mousePos) {
  switch (mouseButton) {
    case 0: {
      float tbRadius = 1.0f;

      Vector3 p0 =
          UnprojectOnTbSurface(g_cam.GetPosition(), lastMousePos, tbRadius);
      Vector3 p1 =
          UnprojectOnTbSurface(g_cam.GetPosition(), mousePos, tbRadius);

      Vector3 axis = p0.Cross(p1);

      if (axis.LengthSquared() < 0.01f) break;

      axis.Normalize();

      float distance = Vector3::Distance(p0, p1);
      float angle = distance / tbRadius;

      if (angle < 0.01f) break;

      g_sphereWorld *= Matrix::CreateFromAxisAngle(axis, -angle);
      break;
    }
    case 1: {
      Vector3 p0 = UnprojectOnTbPlane(g_cam.GetPosition(), lastMousePos);
      Vector3 p1 = UnprojectOnTbPlane(g_cam.GetPosition(), mousePos);

      Vector3 movement = p1 - p0;

      g_sphereWorld *= Matrix::CreateTranslation(movement);
      break;
    }
    default:
      g_apiLogger->critical("Invalid value in switch statement: {}",
                            mouseButton);
  }
}

bool my::DoTest() {
  HRESULT hr = S_OK;

  ID3D11ShaderResourceView* nullSRV[2] = {nullptr, nullptr};
  g_context->PSSetShaderResources(0, 2, nullSRV);

  ID3D11RenderTargetView* renderTargets[2] = {g_posColorRTV.Get(),
                                              g_normalRTV.Get()};
  g_context->OMSetRenderTargets(2, renderTargets, g_depthStencilView.Get());

  float clearColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
  for (int i = 0; i < 2; i++)
    g_context->ClearRenderTargetView(renderTargets[i], clearColor);

  g_context->ClearDepthStencilView(g_depthStencilView.Get(),
                                   D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
                                   1.0f, 0);

  UINT stride = sizeof(Vertex);
  UINT offset = 0;
  g_context->IASetInputLayout(g_inputLayout.Get());
  g_context->IASetVertexBuffers(0, 1, g_vertexBuffer.GetAddressOf(), &stride,
                                &offset);
  g_context->IASetIndexBuffer(g_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
  g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  g_cam.UpdateViewMatrix();

  ObjectConstants objConstants;
  objConstants.view = g_cam.View().Transpose();
  objConstants.proj = g_cam.Proj().Transpose();

  objConstants.world = g_sphereWorld.Transpose();
  objConstants.worldInvTranspose = g_sphereWorld.Invert();

  D3D11_MAPPED_SUBRESOURCE mappedResource;
  ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
  g_context->Map(g_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
                 &mappedResource);
  memcpy(mappedResource.pData, &objConstants, sizeof(objConstants));
  g_context->Unmap(g_constantBuffer.Get(), 0);

  g_context->VSSetShader(g_geometryVS.Get(), nullptr, 0);
  g_context->VSSetConstantBuffers(0, 1, g_constantBuffer.GetAddressOf());
  g_context->PSSetShader(g_geometryPS.Get(), nullptr, 0);

  g_context->OMSetDepthStencilState(g_depthStencilState.Get(), 1);
  g_context->RSSetState(g_rasterizerState.Get());

  g_context->DrawIndexed(g_sphereIndexCount, 0, 0);

  g_context->OMSetRenderTargets(1, g_renderTargetView.GetAddressOf(), nullptr);

  g_context->IASetVertexBuffers(0, 1, g_screenQuadVB.GetAddressOf(), &stride,
                                &offset);
  g_context->IASetIndexBuffer(g_screenQuadIB.Get(), DXGI_FORMAT_R32_UINT, 0);
  g_context->VSSetShader(g_lightVS.Get(), nullptr, 0);

  ID3D11ShaderResourceView* textures[2] = {g_posColorSRV.Get(),
                                           g_normalSRV.Get()};
  g_context->PSSetShaderResources(0, 2, textures);
  g_context->PSSetShader(g_lightPS.Get(), nullptr, 0);

  g_context->DrawIndexed(6, 0, 0);

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
  g_screenQuadVB.Reset();
  g_screenQuadIB.Reset();

  g_posColorRTV.Reset();
  g_posColorSRV.Reset();

  g_normalRTV.Reset();
  g_normalSRV.Reset();

  g_geometryVS.Reset();
  g_geometryPS.Reset();

  g_vertexBuffer.Reset();
  g_indexBuffer.Reset();
  g_constantBuffer.Reset();
  g_lightVS.Reset();
  g_lightPS.Reset();

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

bool my::LoadShaders() {
  HRESULT hr = S_OK;

  std::vector<BYTE> vsByteCode;
  std::vector<BYTE> psByteCode;

  my::ReadData("../MyEngine/hlsl/Geometry_VS.cso", vsByteCode);
  hr = g_device->CreateVertexShader(vsByteCode.data(), vsByteCode.size(),
                                    nullptr,
                                    g_geometryVS.ReleaseAndGetAddressOf());
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
      {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 16,
       D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28,
       D3D11_INPUT_PER_VERTEX_DATA, 0},
  };

  // Create the input layout
  hr = g_device->CreateInputLayout(vertexDesc, 4, vsByteCode.data(),
                                   vsByteCode.size(),
                                   g_inputLayout.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    g_apiLogger->error("CreateInputLayout Failed.");
    return false;
  }

  my::ReadData("../MyEngine/hlsl/Geometry_PS.cso", psByteCode);
  hr =
      g_device->CreatePixelShader(psByteCode.data(), psByteCode.size(), nullptr,
                                  g_geometryPS.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    g_apiLogger->error("CreatePixelShader Failed.");
    return false;
  }

  my::ReadData("../MyEngine/hlsl/Light_VS.cso", vsByteCode);
  hr =
      g_device->CreateVertexShader(vsByteCode.data(), vsByteCode.size(),
                                   nullptr, g_lightVS.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    g_apiLogger->error("CreateVertexShader Failed.");
    return false;
  }

  my::ReadData("../MyEngine/hlsl/Light_PS.cso", psByteCode);
  hr = g_device->CreatePixelShader(psByteCode.data(), psByteCode.size(),
                                   nullptr, g_lightPS.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    g_apiLogger->error("CreatePixelShader Failed.");
    return false;
  }

  return true;
}

bool my::ReadData(const char* name, std::vector<BYTE>& blob) {
  std::ifstream fin(name, std::ios::binary);

  if (!fin) {
    g_apiLogger->error("{} not found.", name);
    return false;
  }

  blob.assign((std::istreambuf_iterator<char>(fin)),
              std::istreambuf_iterator<char>());

  fin.close();

  return true;
}

bool my::BuildScreenQuadGeometryBuffers() {
  HRESULT hr = S_OK;

  GeometryGenerator geoGen;
  GeometryGenerator::MeshData quad =
      geoGen.CreateQuad(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f);

  std::vector<Vertex> vertices(quad.vertices.size());

  for (UINT i = 0; i < quad.vertices.size(); i++) {
    vertices[i].pos = quad.vertices[i].position;
    vertices[i].normal = quad.vertices[i].normal;
    vertices[i].tex = quad.vertices[i].texC;
  }

  D3D11_BUFFER_DESC vertexBufferDesc = {};
  vertexBufferDesc.ByteWidth = sizeof(Vertex) * vertices.size();
  vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
  vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  vertexBufferDesc.CPUAccessFlags = 0;
  vertexBufferDesc.MiscFlags = 0;

  D3D11_SUBRESOURCE_DATA vertexBufferData;
  vertexBufferData.pSysMem = &vertices[0];
  vertexBufferData.SysMemPitch = 0;
  vertexBufferData.SysMemSlicePitch = 0;

  hr = g_device->CreateBuffer(&vertexBufferDesc, &vertexBufferData,
                              g_screenQuadVB.ReleaseAndGetAddressOf());

  if (FAILED(hr)) {
    g_apiLogger->error("CreateBuffer Failed.");
    return false;
  }

  D3D11_BUFFER_DESC indexBufferDesc = {};
  indexBufferDesc.ByteWidth = sizeof(UINT) * quad.indices.size();
  indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
  indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
  indexBufferDesc.CPUAccessFlags = 0;
  indexBufferDesc.MiscFlags = 0;

  D3D11_SUBRESOURCE_DATA indexBufferData = {};
  indexBufferData.pSysMem = &quad.indices[0];
  indexBufferData.SysMemPitch = 0;
  indexBufferData.SysMemSlicePitch = 0;

  hr = g_device->CreateBuffer(&indexBufferDesc, &indexBufferData,
                              g_screenQuadIB.ReleaseAndGetAddressOf());

  if (FAILED(hr)) {
    g_apiLogger->error("CreateBuffer Failed.");
    return false;
  }

  return true;
}

Vector2 my::GetMouseNDC(Vector2 mousePos) {
  return Vector2(+2.0f * mousePos.x / g_renderTargetWidth - 1.0f,
                 -2.0f * mousePos.y / g_renderTargetHeight + 1.0f);
}

Vector3 my::UnprojectOnTbPlane(Vector3 cameraPos, Vector2 mousePos) {
  Vector2 mouseNDC = my::GetMouseNDC(mousePos);

  // unproject cursor on the near plane
  Vector3 rayDir(mouseNDC.x, mouseNDC.y, 0.0f);
  rayDir = Vector3::Transform(rayDir, g_cam.Proj().Invert());

  rayDir.Normalize();  // unprojected ray direction

  //	  camera
  //		|\
  //		| \
  //		|  \
  //	h	|	\
  //		| 	 \
  //		| 	  \
  //	_ _ | _ _ _\ _ _  near plane
  //			l

  float h = rayDir.z;
  float l = sqrtf(rayDir.x * rayDir.x + rayDir.y * rayDir.y);
  float cameraSpehreDistance = Vector3::Distance(cameraPos, Vector3());

  /*
   * calculate intersection point between unprojected ray and the plane
   *|y = mx + q
   *|y = 0
   *
   * x = -q/m
   */
  if (l == 0.0f) {
    // ray aligned with camera
    rayDir = Vector3();
    return rayDir;
  }

  float m = h / l;
  float q = cameraSpehreDistance;
  float x = -q / m;

  float rayLength = sqrtf(q * q + x * x);
  rayDir *= rayLength;
  rayDir.z = 0.0f;
  return rayDir;
}

Vector3 my::UnprojectOnTbSurface(Vector3 cameraPos, Vector2 mousePos,
                                 float tbRadius) {
  // unproject cursor on the near plane
  Vector2 mouseNDC = my::GetMouseNDC(mousePos);

  Vector3 rayDir(mouseNDC.x, mouseNDC.y, 0.0f);
  rayDir = Vector3::Transform(rayDir, g_cam.Proj().Invert());

  rayDir.Normalize();  // unprojected ray direction
  float cameraSpehreDistance = Vector3::Distance(cameraPos, Vector3());
  float radius2 = tbRadius * tbRadius;

  //	  camera
  //		|\
  //		| \
  //		|  \
  //	h	|	\
  //		| 	 \
  //		| 	  \
  //	_ _ | _ _ _\ _ _  near plane
  //			l

  float h = rayDir.z;
  float l = sqrtf(rayDir.x * rayDir.x + rayDir.y * rayDir.y);

  if (l == 0.0f) {
    // ray aligned with camera
    rayDir = Vector3(rayDir.x, rayDir.y, tbRadius);
    return rayDir;
  }

  float m = h / l;
  float q = cameraSpehreDistance;

  /*
   * calculate intersection point between unprojected ray and trackball surface
   *|y = m * x + q
   *|x^2 + y^2 = r^2
   *
   * (m^2 + 1) * x^2 + (2 * m * q) * x + q^2 - r^2 = 0
   */
  float a = m * m + 1.0f;
  float b = 2.0f * m * q;
  float c = q * q - radius2;
  float delta = b * b - (4.0f * a * c);

  if (delta >= 0.0f) {
    // intersection with sphere
    float x = (-b - sqrtf(delta)) / (2.0f * a);
    float y = m * x + q;

    float angle = atan2f(-y, -x) + XM_PI;

    if (angle >= XM_PIDIV4) {
      // if angle between intersection point and X' axis is >= 45��, return that
      // point otherwise, calculate intersection point with hyperboloid

      float rayLength = sqrtf(x * x + (cameraSpehreDistance - y) *
                                          (cameraSpehreDistance - y));
      rayDir *= rayLength;
      rayDir.z += cameraSpehreDistance;
      return rayDir;
    }
  }

  // intersection with hyperboloid
  /*
   *|y = m * x + q
   *|y = (1 / x) * (r^2 / 2)
   *
   * m * x^2 + q * x - r^2 / 2 = 0
   */

  a = m;
  b = q;
  c = -radius2 * 0.5f;
  delta = b * b - (4.0f * a * c);
  float x = (-b - sqrtf(delta)) / (2.0f * a);
  float y = m * x + q;

  float rayLength =
      sqrtf(x * x + (cameraSpehreDistance - y) * (cameraSpehreDistance - y));

  rayDir *= rayLength;
  rayDir.z += cameraSpehreDistance;
  return rayDir;
}