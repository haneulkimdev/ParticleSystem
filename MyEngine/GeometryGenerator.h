#pragma once

#include "MeshData.h"
#include "SimpleMath.h"
#include "Vertex.h"

namespace my {
class GeometryGenerator {
 public:
  MeshData CreateSphere(float radius, UINT sliceCount, UINT stackCount);

  MeshData CreateQuad(float x, float y, float w, float h, float depth);
};
}  // namespace my