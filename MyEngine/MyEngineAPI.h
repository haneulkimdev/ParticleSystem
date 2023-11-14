#pragma once

#define MY_API extern "C" __declspec(dllexport)

namespace my {
MY_API bool InitEngine();

MY_API bool SetRenderTargetSize(int w, int h);

MY_API bool DoTest();
MY_API bool GetRenderTarget();

MY_API void DeinitEngine();
}  // namespace my