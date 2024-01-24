#pragma once

#include <string>
#include <vector>

#include "MeshData.h"

namespace my {
class ModelImporter {
 public:
  void ImportModel(std::string fileName);

  std::vector<MeshData> m_meshData;

  // Transform the data from glTF space to engine-space:
  bool transformToLH = true;
};
}  // namespace my