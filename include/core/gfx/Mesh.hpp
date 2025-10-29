#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include "Vertex.hpp"




class Mesh {
public:
    bool createQuadXY(ID3D11Device* dev, float w = 2.0f, float h = 2.0f, float z = 0.0f);
    bool createCube(ID3D11Device* dev, float half = 0.5f);

    // Getters for rendering data
    ID3D11Buffer* vertexBuffer() const { return m_vb.Get(); }
    ID3D11Buffer* indexBuffer() const { return m_ib.Get(); }
    UINT stride() const { return m_stride; }
    UINT indexCount() const { return m_indexCount; }

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vb;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_ib;
    UINT m_stride = sizeof(VertexPC);
    UINT m_indexCount = 0;
};