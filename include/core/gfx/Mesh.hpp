#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>


struct VertexPNV {
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT3 norm;
    DirectX::XMFLOAT2 uv;
};


class Mesh {
public:
    bool createQuadXY(ID3D11Device* dev, float w = 2.0f, float h = 2.0f, float z = 0.0f);
    void draw(ID3D11DeviceContext* ctx) const;


    UINT indexCount() const { return m_indexCount; }


private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vb;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_ib;
    UINT m_stride = sizeof(VertexPNV);
    UINT m_indexCount = 0;
};