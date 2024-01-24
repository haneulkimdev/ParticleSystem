#include "ModelImporter.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

using namespace DirectX::SimpleMath;

namespace my {
void ModelImporter::ImportModel(std::string path) {
  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, path);

  if (!ret) return;

  // Create meshes:
  for (auto& x : model.meshes) {
    MeshData mesh;

    for (auto& prim : x.primitives) {
      const size_t index_remap[] = {0, 2, 1};

      if (prim.indices >= 0) {
        // Fill indices:
        const tinygltf::Accessor& accessor = model.accessors[prim.indices];
        const tinygltf::BufferView& bufferView =
            model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

        int stride = accessor.ByteStride(bufferView);
        size_t indexCount = accessor.count;
        mesh.indices.resize(indexCount);

        const uint8_t* data =
            buffer.data.data() + accessor.byteOffset + bufferView.byteOffset;

        if (stride == 1) {
          for (size_t i = 0; i < indexCount; i += 3) {
            mesh.indices[i + 0] = data[i + index_remap[0]];
            mesh.indices[i + 1] = data[i + index_remap[1]];
            mesh.indices[i + 2] = data[i + index_remap[2]];
          }
        } else if (stride == 2) {
          for (size_t i = 0; i < indexCount; i += 3) {
            mesh.indices[i + 0] = ((uint16_t*)data)[i + index_remap[0]];
            mesh.indices[i + 1] = ((uint16_t*)data)[i + index_remap[1]];
            mesh.indices[i + 2] = ((uint16_t*)data)[i + index_remap[2]];
          }
        } else if (stride == 4) {
          for (size_t i = 0; i < indexCount; i += 3) {
            mesh.indices[i + 0] = ((uint32_t*)data)[i + index_remap[0]];
            mesh.indices[i + 1] = ((uint32_t*)data)[i + index_remap[1]];
            mesh.indices[i + 2] = ((uint32_t*)data)[i + index_remap[2]];
          }
        } else {
          assert(0 && "unsupported index stride!");
        }
      }

      for (auto& attr : prim.attributes) {
        const std::string& attr_name = attr.first;
        int attr_data = attr.second;

        const tinygltf::Accessor& accessor = model.accessors[attr_data];
        const tinygltf::BufferView& bufferView =
            model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

        int stride = accessor.ByteStride(bufferView);
        size_t vertexCount = accessor.count;
        mesh.vertices.resize(vertexCount);

        const uint8_t* data =
            buffer.data.data() + accessor.byteOffset + bufferView.byteOffset;

        if (!attr_name.compare("POSITION")) {
          for (size_t i = 0; i < vertexCount; i++) {
            mesh.vertices[i].position = *(const Vector3*)(data + i * stride);
          }
        }

        if (!attr_name.compare("NORMAL")) {
          for (size_t i = 0; i < vertexCount; i++) {
            mesh.vertices[i].normal = *(const Vector3*)(data + i * stride);
          }
        }

        if (!attr_name.compare("TEXCOORD_0")) {
          if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
            for (size_t i = 0; i < vertexCount; ++i) {
              const Vector2& tex = *(const Vector2*)((size_t)data + i * stride);

              mesh.vertices[i].texC.x = tex.x;
              mesh.vertices[i].texC.y = tex.y;
            }
          } else if (accessor.componentType ==
                     TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
            for (size_t i = 0; i < vertexCount; ++i) {
              const uint8_t& s = *(uint8_t*)((size_t)data + i * stride + 0);
              const uint8_t& t = *(uint8_t*)((size_t)data + i * stride + 1);

              mesh.vertices[i].texC.x = s / 255.0f;
              mesh.vertices[i].texC.y = t / 255.0f;
            }
          } else if (accessor.componentType ==
                     TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            for (size_t i = 0; i < vertexCount; ++i) {
              const uint16_t& s = *(uint16_t*)((size_t)data + i * stride +
                                               0 * sizeof(uint16_t));
              const uint16_t& t = *(uint16_t*)((size_t)data + i * stride +
                                               1 * sizeof(uint16_t));

              mesh.vertices[i].texC.x = s / 65535.0f;
              mesh.vertices[i].texC.y = t / 65535.0f;
            }
          }
        }
      }

      m_meshData.push_back(mesh);
    }
  }
}
}  // namespace my