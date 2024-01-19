#pragma once

#include "Vertex.h"

namespace my {
struct MeshData {
  std::vector<Vertex> vertices;
  std::vector<UINT> indices;
};
}  // namespace my