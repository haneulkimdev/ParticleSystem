#pragma once

#include <d3d11.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace my {
struct Mesh {
  ComPtr<ID3D11Buffer> vertexBuffer;
  ComPtr<ID3D11Buffer> indexBuffer;

  ComPtr<ID3D11ShaderResourceView> vertexBufferSRV;
  ComPtr<ID3D11ShaderResourceView> indexBufferSRV;

  UINT indexCount = 0;
  UINT vertexCount = 0;
  UINT stride = 0;
  UINT offset = 0;
};
}  // namespace my