#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>
#include <wrl/client.h>

#include <fstream>
#include <vector>

#include "Camera.h"
#include "SimpleMath.h"
#include "spdlog/spdlog.h"

#define MY_API __declspec(dllexport)

using namespace DirectX::SimpleMath;

namespace my {
extern "C" MY_API bool InitEngine(std::shared_ptr<spdlog::logger> spdlogPtr);

extern "C" MY_API bool SetRenderTargetSize(int w, int h);

extern "C" MY_API bool DoTest();
extern "C" MY_API bool GetDX11SharedRenderTarget(
    ID3D11Device* pDevice, ID3D11ShaderResourceView** ppSRView, int& w, int& h);

extern "C" MY_API void DeinitEngine();

extern "C" MY_API bool LoadShaders();

bool GetQuadRendererCB(ID3D11Buffer* quadRendererCB);

bool GetDevice(ID3D11Device* device);
bool GetContext(ID3D11DeviceContext* context);

bool GetApiLogger(std::shared_ptr<spdlog::logger>& logger);

bool GetEnginePath(std::string& enginePath);

bool ReadData(const std::string& name, std::vector<BYTE>& blob);

bool BuildScreenQuadGeometryBuffers();

Color ColorConvertU32ToFloat4(uint32_t color);
uint32_t ColorConvertFloat4ToU32(const Color& color);
}  // namespace my