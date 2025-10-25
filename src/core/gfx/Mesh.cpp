//
// Created by zyzyz on 2025/10/22.
//

#include "core/gfx/Mesh.hpp"

#include <iterator>

#include "core/gfx/Vertex.hpp"

using namespace DirectX;


bool Mesh::createQuadXY(ID3D11Device* dev, float w, float h, float z)
{
    const float hw = w * 0.5f;
    const float hh = h * 0.5f;


    // Facing -Z so the camera at (0,0,-3) looking +Z sees it
    VertexPNV v[4] = {
        { XMFLOAT3(-hw, -hh, z), XMFLOAT3(0, 0, -1), XMFLOAT2(0, 1) },
        { XMFLOAT3(-hw, hh, z), XMFLOAT3(0, 0, -1), XMFLOAT2(0, 0) },
        { XMFLOAT3( hw, hh, z), XMFLOAT3(0, 0, -1), XMFLOAT2(1, 0) },
        { XMFLOAT3( hw, -hh, z), XMFLOAT3(0, 0, -1), XMFLOAT2(1, 1) },
        };


    const uint16_t idx[6] = { 0, 1, 2, 0, 2, 3 };
    m_indexCount = 6;


    D3D11_BUFFER_DESC vbDesc{}; vbDesc.Usage = D3D11_USAGE_DEFAULT; vbDesc.ByteWidth = sizeof(v);
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vinit{}; vinit.pSysMem = v;
    if (FAILED(dev->CreateBuffer(&vbDesc, &vinit, m_vb.ReleaseAndGetAddressOf()))) return false;


    D3D11_BUFFER_DESC ibDesc{}; ibDesc.Usage = D3D11_USAGE_DEFAULT; ibDesc.ByteWidth = sizeof(idx);
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA iinit{}; iinit.pSysMem = idx;
    if (FAILED(dev->CreateBuffer(&ibDesc, &iinit, m_ib.ReleaseAndGetAddressOf()))) return false;


    return true;
}

bool Mesh::createCube(ID3D11Device *dev, float h) {

    // 8 角点 + 每面独立颜色（这里直接给 8 顶点带颜色，简单起见）
    const VertexPC v[] = {
        {{-h,-h,-h},{1,0,0,1}}, {{-h, h,-h},{0,1,0,1}}, {{ h, h,-h},{0,0,1,1}}, {{ h,-h,-h},{1,1,0,1}}, // back (z-)
        {{-h,-h, h},{1,0,1,1}}, {{-h, h, h},{0,1,1,1}}, {{ h, h, h},{1,1,1,1}}, {{ h,-h, h},{0.2f,0.6f,1,1}}, // front (z+)
    };
    const uint16_t idx[] = {
        // back face
        0,1,2, 0,2,3,
        // front face
        4,6,5, 4,7,6,
        // left
        4,5,1, 4,1,0,
        // right
        3,2,6, 3,6,7,
        // top
        1,5,6, 1,6,2,
        // bottom
        4,0,3, 4,3,7
    };
    m_indexCount = (UINT)std::size(idx);

    D3D11_BUFFER_DESC bd{}; D3D11_SUBRESOURCE_DATA sd{};
    bd.Usage = D3D11_USAGE_DEFAULT;

    bd.ByteWidth = (UINT)sizeof(v);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; sd.pSysMem = v;
    if (FAILED(dev->CreateBuffer(&bd, &sd, m_vb.ReleaseAndGetAddressOf()))) return false;

    bd.ByteWidth = (UINT)sizeof(idx);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER; sd.pSysMem = idx;
    if (FAILED(dev->CreateBuffer(&bd, &sd, m_ib.ReleaseAndGetAddressOf()))) return false;

    return true;
}


void Mesh::draw(ID3D11DeviceContext* ctx) const
{
    UINT stride = m_stride, offset = 0;
    ID3D11Buffer* vb = m_vb.Get();
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetIndexBuffer(m_ib.Get(), DXGI_FORMAT_R16_UINT, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->DrawIndexed(m_indexCount, 0, 0);
}
