#include "GeometryGenerator.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

namespace my {
MeshData GeometryGenerator::CreateSphere(float radius, UINT sliceCount,
                                         UINT stackCount) {
  MeshData meshData;

  Vertex topVertex{Vector3(0.0f, +radius, 0.0f), Vector3(0.0f, +1.0f, 0.0f),
                   Vector2(0.0f, 0.0f)};
  Vertex bottomVertex{Vector3(0.0f, -radius, 0.0f), Vector3(0.0f, -1.0f, 0.0f),
                      Vector2(0.0f, 1.0f)};

  meshData.vertices.push_back(topVertex);

  float phiStep = XM_PI / stackCount;
  float thetaStep = 2.0f * XM_PI / sliceCount;

  for (size_t i = 1; i <= stackCount - 1; i++) {
    float phi = i * phiStep;

    for (size_t j = 0; j <= sliceCount; j++) {
      float theta = j * thetaStep;

      Vertex v;

      v.position.x = radius * sinf(phi) * cosf(theta);
      v.position.y = radius * cosf(phi);
      v.position.z = radius * sinf(phi) * sinf(theta);

      v.normal = v.position;
      v.normal.Normalize();

      v.texC.x = theta / XM_2PI;
      v.texC.y = phi / XM_PI;

      meshData.vertices.push_back(v);
    }
  }

  meshData.vertices.push_back(bottomVertex);

  for (UINT i = 1; i <= sliceCount; i++) {
    meshData.indices.push_back(0);
    meshData.indices.push_back(i + 1);
    meshData.indices.push_back(i);
  }

  UINT baseIndex = 1;
  UINT ringVertexCount = sliceCount + 1;
  for (UINT i = 0; i < stackCount - 2; i++) {
    for (UINT j = 0; j < sliceCount; j++) {
      meshData.indices.push_back(baseIndex + i * ringVertexCount + j);
      meshData.indices.push_back(baseIndex + i * ringVertexCount + j + 1);
      meshData.indices.push_back(baseIndex + (i + 1) * ringVertexCount + j);

      meshData.indices.push_back(baseIndex + (i + 1) * ringVertexCount + j);
      meshData.indices.push_back(baseIndex + i * ringVertexCount + j + 1);
      meshData.indices.push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);
    }
  }

  UINT southPoleIndex = (UINT)meshData.vertices.size() - 1;

  baseIndex = southPoleIndex - ringVertexCount;

  for (UINT i = 0; i < sliceCount; i++) {
    meshData.indices.push_back(southPoleIndex);
    meshData.indices.push_back(baseIndex + i);
    meshData.indices.push_back(baseIndex + i + 1);
  }

  return meshData;
}

MeshData GeometryGenerator::CreateQuad(float x, float y, float w, float h,
                                       float depth) {
  MeshData meshData;

  meshData.vertices.resize(4);
  meshData.indices.resize(6);

  // Position coordinates specified in NDC space.
  meshData.vertices[0] =
      Vertex{Vector3(x, y - h, depth), Vector3(0.0f, 0.0f, -1.0f),
             Vector2(0.0f, 1.0f)};

  meshData.vertices[1] = Vertex{
      Vector3(x, y, depth), Vector3(0.0f, 0.0f, -1.0f), Vector2(0.0f, 0.0f)};

  meshData.vertices[2] =
      Vertex{Vector3(x + w, y, depth), Vector3(0.0f, 0.0f, -1.0f),
             Vector2(1.0f, 0.0f)};

  meshData.vertices[3] =
      Vertex{Vector3(x + w, y - h, depth), Vector3(0.0f, 0.0f, -1.0f),
             Vector2(1.0f, 1.0f)};

  meshData.indices[0] = 0;
  meshData.indices[1] = 1;
  meshData.indices[2] = 2;

  meshData.indices[3] = 0;
  meshData.indices[4] = 2;
  meshData.indices[5] = 3;

  return meshData;
}
}  // namespace my