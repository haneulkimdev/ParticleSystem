#include "GeometryGenerator.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

namespace my {
MeshData GeometryGenerator::CreateBox(float width, float height, float depth) {
  MeshData meshData;

  Vertex v[24];

  float w2 = 0.5f * width;
  float h2 = 0.5f * height;
  float d2 = 0.5f * depth;

  // Fill in the front face vertex data.
  v[0] = Vertex{Vector3(-w2, -h2, -d2), Vector3(0.0f, 0.0f, -1.0f),
                Vector2(0.0f, 1.0f)};
  v[1] = Vertex{Vector3(-w2, +h2, -d2), Vector3(0.0f, 0.0f, -1.0f),
                Vector2(0.0f, 0.0f)};
  v[2] = Vertex{Vector3(+w2, +h2, -d2), Vector3(0.0f, 0.0f, -1.0f),
                Vector2(1.0f, 0.0f)};
  v[3] = Vertex{Vector3(+w2, -h2, -d2), Vector3(0.0f, 0.0f, -1.0f),
                Vector2(1.0f, 1.0f)};

  // Fill in the back face vertex data.
  v[4] = Vertex{Vector3(-w2, -h2, +d2), Vector3(0.0f, 0.0f, 1.0f),
                Vector2(1.0f, 1.0f)};
  v[5] = Vertex{Vector3(+w2, -h2, +d2), Vector3(0.0f, 0.0f, 1.0f),
                Vector2(0.0f, 1.0f)};
  v[6] = Vertex{Vector3(+w2, +h2, +d2), Vector3(0.0f, 0.0f, 1.0f),
                Vector2(0.0f, 0.0f)};
  v[7] = Vertex{Vector3(-w2, +h2, +d2), Vector3(0.0f, 0.0f, 1.0f),
                Vector2(1.0f, 0.0f)};

  // Fill in the top face vertex data.
  v[8] = Vertex{Vector3(-w2, +h2, -d2), Vector3(0.0f, 1.0f, 0.0f),
                Vector2(0.0f, 1.0f)};
  v[9] = Vertex{Vector3(-w2, +h2, +d2), Vector3(0.0f, 1.0f, 0.0f),
                Vector2(0.0f, 0.0f)};
  v[10] = Vertex{Vector3(+w2, +h2, +d2), Vector3(0.0f, 1.0f, 0.0f),
                 Vector2(1.0f, 0.0f)};
  v[11] = Vertex{Vector3(+w2, +h2, -d2), Vector3(0.0f, 1.0f, 0.0f),
                 Vector2(1.0f, 1.0f)};

  // Fill in the bottom face vertex data.
  v[12] = Vertex{Vector3(-w2, -h2, -d2), Vector3(0.0f, -1.0f, 0.0f),
                 Vector2(1.0f, 1.0f)};
  v[13] = Vertex{Vector3(+w2, -h2, -d2), Vector3(0.0f, -1.0f, 0.0f),
                 Vector2(0.0f, 1.0f)};
  v[14] = Vertex{Vector3(+w2, -h2, +d2), Vector3(0.0f, -1.0f, 0.0f),
                 Vector2(0.0f, 0.0f)};
  v[15] = Vertex{Vector3(-w2, -h2, +d2), Vector3(0.0f, -1.0f, 0.0f),
                 Vector2(1.0f, 0.0f)};

  // Fill in the left face vertex data.
  v[16] = Vertex{Vector3(-w2, -h2, +d2), Vector3(-1.0f, 0.0f, 0.0f),
                 Vector2(0.0f, 1.0f)};
  v[17] = Vertex{Vector3(-w2, +h2, +d2), Vector3(-1.0f, 0.0f, 0.0f),
                 Vector2(0.0f, 0.0f)};
  v[18] = Vertex{Vector3(-w2, +h2, -d2), Vector3(-1.0f, 0.0f, 0.0f),
                 Vector2(1.0f, 0.0f)};
  v[19] = Vertex{Vector3(-w2, -h2, -d2), Vector3(-1.0f, 0.0f, 0.0f),
                 Vector2(1.0f, 1.0f)};

  // Fill in the right face vertex data.
  v[20] = Vertex{Vector3(+w2, -h2, -d2), Vector3(1.0f, 0.0f, 0.0f),
                 Vector2(0.0f, 1.0f)};
  v[21] = Vertex{Vector3(+w2, +h2, -d2), Vector3(1.0f, 0.0f, 0.0f),
                 Vector2(0.0f, 0.0f)};
  v[22] = Vertex{Vector3(+w2, +h2, +d2), Vector3(1.0f, 0.0f, 0.0f),
                 Vector2(1.0f, 0.0f)};
  v[23] = Vertex{Vector3(+w2, -h2, +d2), Vector3(1.0f, 0.0f, 0.0f),
                 Vector2(1.0f, 1.0f)};

  meshData.vertices.assign(&v[0], &v[24]);

  //
  // Create the indices.
  //

  UINT i[36];

  // Fill in the front face index data
  i[0] = 0;
  i[1] = 1;
  i[2] = 2;
  i[3] = 0;
  i[4] = 2;
  i[5] = 3;

  // Fill in the back face index data
  i[6] = 4;
  i[7] = 5;
  i[8] = 6;
  i[9] = 4;
  i[10] = 6;
  i[11] = 7;

  // Fill in the top face index data
  i[12] = 8;
  i[13] = 9;
  i[14] = 10;
  i[15] = 8;
  i[16] = 10;
  i[17] = 11;

  // Fill in the bottom face index data
  i[18] = 12;
  i[19] = 13;
  i[20] = 14;
  i[21] = 12;
  i[22] = 14;
  i[23] = 15;

  // Fill in the left face index data
  i[24] = 16;
  i[25] = 17;
  i[26] = 18;
  i[27] = 16;
  i[28] = 18;
  i[29] = 19;

  // Fill in the right face index data
  i[30] = 20;
  i[31] = 21;
  i[32] = 22;
  i[33] = 20;
  i[34] = 22;
  i[35] = 23;

  meshData.indices.assign(&i[0], &i[36]);

  return meshData;
}
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

std::vector<MeshData> GeometryGenerator::LoadModel(
    const std::string& filename) {
  ModelImporter importer;
  importer.ImportModel(filename);
  return importer.m_meshData;
}
}  // namespace my