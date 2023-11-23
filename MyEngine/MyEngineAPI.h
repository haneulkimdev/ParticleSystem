#pragma once

#define MY_API extern "C" __declspec(dllexport)

namespace spdlog {
class logger;
}

struct ID3D11Device;
struct ID3D11ShaderResourceView;

namespace my {
MY_API bool InitEngine(spdlog::logger* spdlogPtr);

MY_API bool SetRenderTargetSize(int w, int h);

MY_API bool DoTest();
MY_API bool GetRenderTarget(ID3D11Device* device,
                            ID3D11ShaderResourceView** textureView);

MY_API void DeinitEngine();
}  // namespace my