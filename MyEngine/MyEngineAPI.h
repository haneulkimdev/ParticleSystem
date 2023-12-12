#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <fstream>
#include <vector>

#include "ShaderInterop.h"
#include "SimpleMath.h"
#include "spdlog/spdlog.h"

#define MY_API extern "C" __declspec(dllexport)

namespace my {
MY_API bool InitEngine(std::shared_ptr<spdlog::logger> spdlogPtr);

MY_API bool SetRenderTargetSize(int w, int h);

MY_API bool DoTest();
MY_API bool GetDX11SharedRenderTarget(ID3D11Device* dx11ImGuiDevice,
                                      ID3D11ShaderResourceView** sharedSRV,
                                      int& w, int& h);

MY_API void DeinitEngine();

MY_API bool LoadShaders();

bool GetEnginePath(std::string& enginePath);

bool ReadData(const std::string& name, std::vector<BYTE>& blob);

bool BuildScreenQuadGeometryBuffers();
}  // namespace my