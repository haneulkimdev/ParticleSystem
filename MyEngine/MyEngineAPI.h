#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <fstream>
#include <vector>

#include "Camera.h"
#include "ShaderInterop.h"
#include "SimpleMath.h"
#include "spdlog/spdlog.h"

#define MY_API extern "C" __declspec(dllexport)

namespace my {
MY_API bool InitEngine(std::shared_ptr<spdlog::logger> spdlogPtr);

MY_API bool SetRenderTargetSize(int w, int h);

MY_API UINT GetMaxParticleCount();

MY_API float GetParticleSize(int index);
MY_API Color GetParticleColor(int index);

MY_API Vector3 GetLightPosition();
MY_API float GetLightIntensity();
MY_API Color GetLightColor();

MY_API Vector3 GetDistBoxCenter();
MY_API float GetDistBoxSize();

MY_API float GetSmoothingCoefficient();

MY_API void SetParticleSize(int index, float size);
MY_API void SetParticleColor(int index, const Color& color);

MY_API void SetLightPosition(const Vector3& position);
MY_API void SetLightIntensity(float intensity);
MY_API void SetLightColor(const Color& color);

MY_API void SetDistBoxCenter(const Vector3& center);
MY_API void SetDistBoxSize(float size);

MY_API void SetSmoothingCoefficient(float smoothingCoefficient);

MY_API void UpdateParticles(float dt);

MY_API bool DoTest();
MY_API bool GetDX11SharedRenderTarget(ID3D11Device* dx11ImGuiDevice,
                                      ID3D11ShaderResourceView** sharedSRV,
                                      int& w, int& h);

MY_API void DeinitEngine();

MY_API bool LoadShaders();

bool GetEnginePath(std::string& enginePath);

bool ReadData(const std::string& name, std::vector<BYTE>& blob);

bool BuildScreenQuadGeometryBuffers();

Color ColorConvertU32ToFloat4(UINT color);
UINT ColorConvertFloat4ToU32(const Color& color);
}  // namespace my