#pragma once

#include "SimpleMath.h"

namespace my {
struct Vertex {
  DirectX::SimpleMath::Vector3 position;
  DirectX::SimpleMath::Vector3 normal;
  DirectX::SimpleMath::Vector2 texC;
};
}  // namespace my