#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <fstream>
#include <vector>

#include "GeometryGenerator.h"
#include "spdlog/spdlog.h"

#define MY_API extern "C" __declspec(dllexport)

namespace my {
MY_API bool InitEngine(spdlog::logger* spdlogPtr);

MY_API bool SetRenderTargetSize(int w, int h);

MY_API void UpdateScene(DirectX::SimpleMath::Vector2 mouseDragDeltaLeft,
                        DirectX::SimpleMath::Vector2 mouseDragDeltaRight);
MY_API bool DoTest();
MY_API bool GetRenderTarget(ID3D11Device* device,
                            ID3D11ShaderResourceView** textureView);

MY_API void DeinitEngine();

bool ReadData(const char* name, std::vector<BYTE>& blob);

bool BuildScreenQuadGeometryBuffers();
}  // namespace my