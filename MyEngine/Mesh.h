#pragma once

#include <d3d11.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace my {
class Mesh {
 public:
  ~Mesh() {
    vertexBuffer.Reset();
    indexBuffer.Reset();
    vertexBufferSRV.Reset();
    indexBufferSRV.Reset();
  }

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