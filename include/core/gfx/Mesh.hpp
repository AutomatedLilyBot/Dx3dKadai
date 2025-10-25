#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include "Vertex.hpp"


struct VertexPNV {
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT3 norm;
    DirectX::XMFLOAT2 uv;
};


class Mesh {
public:
    bool createQuadXY(ID3D11Device* dev, float w = 2.0f, float h = 2.0f, float z = 0.0f);
    bool createCube(ID3D11Device* dev, float half = 0.5f);

    void draw(ID3D11DeviceContext* ctx) const;
    UINT indexCount() const { return m_indexCount; }


private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vb;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_ib;
    UINT m_stride = sizeof(VertexPC);
    UINT m_indexCount = 0;
};