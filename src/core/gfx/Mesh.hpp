#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <vector>

#include "Vertex.hpp"

class Mesh {
public:
    bool createQuadXY(ID3D11Device *dev, float w = 2.0f, float h = 2.0f, float z = 0.0f);

    bool createCube(ID3D11Device *dev, float half = 0.5f);

    // Create from VertexPNCT data and 16-bit indices
    bool createFromPNCT(ID3D11Device *dev,
                        const std::vector<VertexPNCT> &vertices,
                        const std::vector<uint16_t> &indices);

    // Getters for rendering data
    ID3D11Buffer *vertexBuffer() const { return m_vb.Get(); }
    ID3D11Buffer *indexBuffer() const { return m_ib.Get(); }
    UINT stride() const { return m_stride; }
    UINT indexCount() const { return m_indexCount; }

    // CPU-side vertex data access (for collision/physics)
    const std::vector<VertexPNCT> &vertices() const { return m_vertices; }
    const std::vector<uint16_t> &indices() const { return m_indices; }

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vb;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_ib;
    UINT m_stride = sizeof(VertexPC);
    UINT m_indexCount = 0;

    // CPU-side copy for collision detection and other CPU operations
    std::vector<VertexPNCT> m_vertices;
    std::vector<uint16_t> m_indices;
};