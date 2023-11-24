#pragma once

#include "SimpleMath.h"

class GeometryGenerator {
 public:
  struct Vertex {
    DirectX::SimpleMath::Vector3 position;
    DirectX::SimpleMath::Vector3 normal;
  };

  struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<UINT> indices;
  };

  MeshData CreateSphere(float radius, UINT sliceCount, UINT stackCount);
};