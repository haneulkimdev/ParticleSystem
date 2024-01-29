#include "Model.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX::SimpleMath;

namespace my {
void Model::Initialize(const ComPtr<ID3D11Device>& device,
                       const ComPtr<ID3D11DeviceContext>& context,
                       const std::string& fileName) {
  auto models = GeometryGenerator::LoadModel(fileName);

  Initialize(device, context, models);
}

void Model::Initialize(const ComPtr<ID3D11Device>& device,
                       const ComPtr<ID3D11DeviceContext>& context,
                       const std::vector<MeshData>& models) {
  D3D11_BUFFER_DESC bd = {};
  bd.ByteWidth = sizeof(objectConstants);
  bd.Usage = D3D11_USAGE_DYNAMIC;
  bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  D3D11_SUBRESOURCE_DATA initData = {};
  initData.pSysMem = &m_objectConstants;

  device->CreateBuffer(&bd, &initData, &m_objectCB);

  bd.ByteWidth = sizeof(materialConstants);
  initData.pSysMem = &m_materialConstants;

  device->CreateBuffer(&bd, &initData, &m_materialCB);

  for (const auto& x : models) {
    std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();

    mesh->vertexCount = x.vertices.size();
    mesh->indexCount = x.indices.size();

    mesh->stride = sizeof(Vector3);
    mesh->offset = 0;

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(Vector3) * mesh->vertexCount;
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE;
    bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;

    std::vector<Vector3> vertices(mesh->vertexCount);
    for (size_t i = 0; i < mesh->vertexCount; i++) {
      vertices[i] = x.vertices[i].position;
    }

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices.data();

    device->CreateBuffer(&bd, &initData, &mesh->vertexBuffer);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    srvDesc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
    srvDesc.BufferEx.NumElements = mesh->vertexCount * 3;

    device->CreateShaderResourceView(mesh->vertexBuffer.Get(), &srvDesc,
                                     &mesh->vertexBufferSRV);

    bd.ByteWidth = sizeof(UINT) * mesh->indexCount;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER | D3D11_BIND_SHADER_RESOURCE;

    initData.pSysMem = x.indices.data();

    device->CreateBuffer(&bd, &initData, &mesh->indexBuffer);

    srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    srvDesc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
    srvDesc.BufferEx.NumElements = mesh->indexCount;

    device->CreateShaderResourceView(mesh->indexBuffer.Get(), &srvDesc,
                                     &mesh->indexBufferSRV);

    m_meshes.push_back(mesh);
  }
}

void Model::Update(const ComPtr<ID3D11Device>& device,
                   const ComPtr<ID3D11DeviceContext>& context) {
  m_objectConstants.world = m_transform.Transpose();

  D3D11_MAPPED_SUBRESOURCE mappedResource = {};
  context->Map(m_objectCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
               &mappedResource);
  memcpy(mappedResource.pData, &m_objectConstants, sizeof(objectConstants));
  context->Unmap(m_objectCB.Get(), 0);

  context->Map(m_materialCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
               &mappedResource);
  memcpy(mappedResource.pData, &m_materialConstants, sizeof(materialConstants));
  context->Unmap(m_materialCB.Get(), 0);

  context->VSSetConstantBuffers(10, 1, m_objectCB.GetAddressOf());
  context->PSSetConstantBuffers(10, 1, m_materialCB.GetAddressOf());
}

void Model::Draw(const ComPtr<ID3D11DeviceContext>& context) {
  for (const auto& x : m_meshes) {
    context->IASetVertexBuffers(0, 1, x->vertexBuffer.GetAddressOf(),
                                &x->stride, &x->offset);
    context->IASetIndexBuffer(x->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

    context->DrawIndexed(x->indexCount, 0, 0);
  }
}
}  // namespace my