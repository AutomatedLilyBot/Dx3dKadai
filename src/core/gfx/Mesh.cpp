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
    // Define 8 unique vertices with proper normals and UVs for each face
    // We need 24 vertices (4 per face * 6 faces) for proper normals per face
    const VertexPNCT v[] = {
        // Back face (Z-)
        {{-h,-h,-h}, {0,0,-1}, {1,0,0,1}, {1.0/3,2.0/3}},
        {{-h, h,-h}, {0,0,-1}, {1,0,0,1}, {1.0/3,1.0/3}},
        {{ h, h,-h}, {0,0,-1}, {1,0,0,1}, {2.0/3,1.0/3}},
        {{ h,-h,-h}, {0,0,-1}, {1,0,0,1}, {2.0/3,2.0/3}},

        // Front face (Z+)
        {{-h,-h, h}, {0,0,1}, {1,0,1,1}, {2.0/3,2.0/3}},
        {{-h, h, h}, {0,0,1}, {1,0,1,1}, {2.0/3,1.0/3}},
        {{ h, h, h}, {0,0,1}, {1,0,1,1}, {1.0/3,1.0/3}},
        {{ h,-h, h}, {0,0,1}, {1,0,1,1}, {1.0/3,2.0/3}},

        // Left face (X-)
        {{-h,-h, h}, {-1,0,0}, {1,1,1,1}, {1.0/3,2.0/3}},
        {{-h, h, h}, {-1,0,0}, {1,1,1,1}, {1.0/3,1.0/3}},
        {{-h, h,-h}, {-1,0,0}, {1,1,1,1}, {2.0/3,1.0/3}},
        {{-h,-h,-h}, {-1,0,0}, {1,1,1,1}, {2.0/3,2.0/3}},

        // Right face (X+)
        {{ h,-h,-h}, {1,0,0}, {0,1,1,1}, {1.0/3,2.0/3}},
        {{ h, h,-h}, {1,0,0}, {0,1,1,1}, {1.0/3,1.0/3}},
        {{ h, h, h}, {1,0,0}, {0,1,1,1}, {2.0/3,1.0/3}},
        {{ h,-h, h}, {1,0,0}, {0,1,1,1}, {2.0/3,2.0/3}},

        // Top face (Y+)
        {{-h, h,-h}, {0,1,0}, {1,1,1,1}, {0,1.0/3}},
        {{-h, h, h}, {0,1,0}, {1,1,1,1}, {0,0}},
        {{ h, h, h}, {0,1,0}, {1,1,1,1}, {1.0/3,0}},
        {{ h, h,-h}, {0,1,0}, {1,1,1,1}, {1.0/3,1.0/3}},

        // Bottom face (Y-)
        {{-h,-h, h}, {0,-1,0}, {1,1,1,1}, {2.0/3,1}},
        {{-h,-h,-h}, {0,-1,0}, {1,1,1,1}, {2.0/3,2.0/3}},
        {{ h,-h,-h}, {0,-1,0}, {1,1,1,1}, {1,2.0/3}},
        {{ h,-h, h}, {0,-1,0}, {1,1,1,1}, {1,1}},
    };

   const uint16_t idx[] = {
        // Back face (Z-) - clockwise from outside (camera view)
        0,2,1, 0,3,2,
        // Front face (Z+) - clockwise from outside
        4,5,6, 4,6,7,
        // Left face (X-) - clockwise from outside
        8,10,9, 8,11,10,
        // Right face (X+) - clockwise from outside
        12,14,13, 12,15,14,
        // Top face (Y+) - clockwise from outside
        16,18,17, 16,19,18,
        // Bottom face (Y-) - clockwise from outside
        20,22,21, 20,23,22
    };



    m_indexCount = (UINT)std::size(idx);
    m_stride = sizeof(VertexPNCT);

    D3D11_BUFFER_DESC bd{};
    D3D11_SUBRESOURCE_DATA sd{};
    bd.Usage = D3D11_USAGE_DEFAULT;

    bd.ByteWidth = (UINT)sizeof(v);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; sd.pSysMem = v;
    if (FAILED(dev->CreateBuffer(&bd, &sd, m_vb.ReleaseAndGetAddressOf()))) return false;

    bd.ByteWidth = (UINT)sizeof(idx);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER; sd.pSysMem = idx;
    if (FAILED(dev->CreateBuffer(&bd, &sd, m_ib.ReleaseAndGetAddressOf()))) return false;

    return true;
}


