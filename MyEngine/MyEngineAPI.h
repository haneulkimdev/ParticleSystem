#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>
#include <wrl/client.h>

#include <fstream>
#include <vector>

#include "Camera.h"
#include "EmittedParticleSystem.h"
#include "Helper.h"
#include "SimpleMath.h"
#include "spdlog/spdlog.h"

#define MY_API __declspec(dllexport)

namespace my {
extern "C" MY_API bool InitEngine(std::shared_ptr<spdlog::logger> spdlogPtr);

extern "C" MY_API bool SetRenderTargetSize(int w, int h);

extern "C" MY_API void Update(float dt);
extern "C" MY_API bool DoTest();
extern "C" MY_API bool GetDX11SharedRenderTarget(
    ID3D11Device* pDevice, ID3D11ShaderResourceView** ppSRView, int& w, int& h);

extern "C" MY_API void DeinitEngine();

extern "C" MY_API bool LoadShaders();

extern "C" MY_API void GetEmitter(EmittedParticleSystem** emitter);

bool BuildScreenQuadGeometryBuffers();

DirectX::SimpleMath::Color ColorConvertU32ToFloat4(uint32_t color);
uint32_t ColorConvertFloat4ToU32(const DirectX::SimpleMath::Color& color);
}  // namespace my