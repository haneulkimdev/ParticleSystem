#pragma once

#include <string>

#include "MeshData.h"
#include "ModelImporter.h"
#include "SimpleMath.h"
#include "Vertex.h"

namespace my {
class GeometryGenerator {
 public:
  static MeshData CreateBox(float width, float height, float depth);

  static MeshData CreateSphere(float radius, UINT sliceCount, UINT stackCount);

  static MeshData CreateQuad(float x, float y, float w, float h, float depth);

  static std::vector<MeshData> LoadModel(const std::string& filename);
};
}  // namespace my