#pragma once

#include "SimpleMath.h"

class GeometryGenerator {
 public:
  struct Vertex {
    DirectX::SimpleMath::Vector3 position;
    DirectX::SimpleMath::Vector3 normal;
    DirectX::SimpleMath::Vector2 texC;
  };

  struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<UINT> indices;
  };

  MeshData CreateSphere(float radius, UINT sliceCount, UINT stackCount);

  MeshData CreateQuad(float x, float y, float w, float h, float depth);
};