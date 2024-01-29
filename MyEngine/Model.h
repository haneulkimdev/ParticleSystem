#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <iostream>  // for debug
#include <memory>
#include <string>
#include <vector>

#include "GeometryGenerator.h"
#include "Mesh.h"
#include "MeshData.h"
#include "SimpleMath.h"

struct objectConstants {
  DirectX::SimpleMath::Matrix world;
};

struct materialConstants {
  DirectX::SimpleMath::Vector4 diffuseAlbedo = {1.0f, 1.0f, 1.0f, 1.0f};
  DirectX::SimpleMath::Vector3 fresnelR0 = {0.01f, 0.01f, 0.01f};
  float roughness = 0.25f;
};

namespace my {
class Model {
 public:
  ~Model() {
    m_meshes.clear();

    m_objectCB.Reset();
    m_materialCB.Reset();
  }

  void Initialize(const Microsoft::WRL::ComPtr<ID3D11Device>& device,
                  const Microsoft::WRL::ComPtr<ID3D11DeviceContext>& context,
                  const std::string& fileName);

  void Initialize(const Microsoft::WRL::ComPtr<ID3D11Device>& device,
                  const Microsoft::WRL::ComPtr<ID3D11DeviceContext>& context,
                  const std::vector<MeshData>& models);

  void Update(const Microsoft::WRL::ComPtr<ID3D11Device>& device,
              const Microsoft::WRL::ComPtr<ID3D11DeviceContext>& context);

  void Draw(const Microsoft::WRL::ComPtr<ID3D11DeviceContext>& context);

  DirectX::SimpleMath::Matrix m_transform;

  std::vector<std::shared_ptr<Mesh>> m_meshes;

 private:
  objectConstants m_objectConstants;
  materialConstants m_materialConstants;

  Microsoft::WRL::ComPtr<ID3D11Buffer> m_objectCB;
  Microsoft::WRL::ComPtr<ID3D11Buffer> m_materialCB;
};
}  // namespace my