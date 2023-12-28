#pragma once

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

MY_API uint32_t GetMaxParticleCount();

MY_API Vector3 GetParticlePosition(int index);
MY_API float GetParticleSize(int index);
MY_API Color GetParticleColor(int index);

MY_API Vector3 GetLightPosition();
MY_API float GetLightIntensity();
MY_API Color GetLightColor();

MY_API Vector3 GetDistBoxCenter();
MY_API float GetDistBoxSize();

MY_API float GetSmoothingCoefficient();

MY_API void SetParticlePosition(int index, const Vector3& position);
MY_API void SetParticleSize(int index, float size);
MY_API void SetParticleColor(int index, const Color& color);

MY_API void SetLightPosition(const Vector3& position);
MY_API void SetLightIntensity(float intensity);
MY_API void SetLightColor(const Color& color);

MY_API void SetDistBoxCenter(const Vector3& center);
MY_API void SetDistBoxSize(float size);

MY_API void SetSmoothingCoefficient(float smoothingCoefficient);

MY_API void UpdateParticleBuffer();

extern "C" MY_API bool DoTest();
extern "C" MY_API bool GetDX11SharedRenderTarget(
    ID3D11Device* dx11ImGuiDevice, ID3D11ShaderResourceView** sharedSRV, int& w,
    int& h);

extern "C" MY_API void DeinitEngine();

extern "C" MY_API bool LoadShaders();

bool GetDevice(ID3D11Device* device);
bool GetContext(ID3D11DeviceContext* context);

bool GetEnginePath(std::string& enginePath);

bool ReadData(const std::string& name, std::vector<BYTE>& blob);

bool BuildScreenQuadGeometryBuffers();

Color ColorConvertU32ToFloat4(uint32_t color);
uint32_t ColorConvertFloat4ToU32(const Color& color);
}  // namespace my