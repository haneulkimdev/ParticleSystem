#include "MyEngineAPI.h"

#include <d3d11.h>
#include <wrl/client.h>

#include <iostream>

using Microsoft::WRL::ComPtr;

static ComPtr<ID3D11Device> g_device;
static ComPtr<ID3D11DeviceContext> g_context;

static ComPtr<ID3D11Texture2D> g_renderTargetBuffer;
static ComPtr<ID3D11RenderTargetView> g_renderTargetView;

bool my::InitEngine() {
  // Create the device and device context.

  UINT createDeviceFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
  createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

  D3D_FEATURE_LEVEL featureLevel;
  HRESULT hr = D3D11CreateDevice(nullptr,  // default adapter
                                 D3D_DRIVER_TYPE_HARDWARE,
                                 nullptr,  // no software device
                                 createDeviceFlags, nullptr,
                                 0,  // default feature level array
                                 D3D11_SDK_VERSION, g_device.GetAddressOf(),
                                 &featureLevel, g_context.GetAddressOf());

  if (FAILED(hr)) {
    std::cout << "D3D11CreateDevice Failed." << std::endl;
    return false;
  }

  if (featureLevel != D3D_FEATURE_LEVEL_11_0) {
    std::cout << "Direct3D Feature Level 11 unsupported." << std::endl;
    return false;
  }

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
  renderTargetBufferDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
  renderTargetBufferDesc.CPUAccessFlags = 0;
  renderTargetBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

  hr = g_device->CreateTexture2D(&renderTargetBufferDesc, nullptr,
                                 g_renderTargetBuffer.GetAddressOf());

  if (FAILED(hr)) {
    std::cout << "CreateTexture2D Failed." << std::endl;
    return false;
  }

  D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc = {};

  renderTargetViewDesc.Format = renderTargetBufferDesc.Format;
  renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
  renderTargetViewDesc.Texture2D.MipSlice = 0;

  hr = g_device->CreateRenderTargetView(g_renderTargetBuffer.Get(),
                                        &renderTargetViewDesc,
                                        g_renderTargetView.GetAddressOf());

  if (FAILED(hr)) {
    std::cout << "CreateRenderTargetView Failed." << std::endl;
    return false;
  }

  return true;
}

bool my::DoTest() { return false; }

bool my::GetRenderTarget() { return false; }

void my::DeinitEngine() {
  g_renderTargetView.Reset();
  g_renderTargetBuffer.Reset();

  g_context.Reset();
  g_device.Reset();
}