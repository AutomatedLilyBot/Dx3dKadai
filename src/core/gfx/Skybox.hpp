#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <vector>
#include <string>
#include "Camera.hpp"
#include "ShaderProgram.hpp"
#include "RenderDeviceD3D11.hpp"

class Skybox {
public:
    Skybox() = default;

    ~Skybox() = default;

    // Initialize skybox mesh (unit cube with inward-facing normals)
    bool initialize(ID3D11Device *device);

    // Load cube map texture from 6 separate files
    // Order: +X (right), -X (left), +Y (top), -Y (bottom), +Z (front), -Z (back)
    bool loadCubeMap(ID3D11Device *device,
                     const std::wstring &rightPath,
                     const std::wstring &leftPath,
                     const std::wstring &topPath,
                     const std::wstring &bottomPath,
                     const std::wstring &frontPath,
                     const std::wstring &backPath);

    // Load cube map texture from DDS file containing all 6 faces
    bool loadCubeMapFromDDS(ID3D11Device *device, const std::wstring &ddsPath);

    // Create a solid color cube map (for testing or default fallback)
    bool createSolidColorCubeMap(ID3D11Device *device,
                                 unsigned char r = 255,
                                 unsigned char g = 255,
                                 unsigned char b = 255,
                                 unsigned char a = 255);

    // Render skybox (should be called before rendering other objects)
    void render(ID3D11DeviceContext *context,
                const Camera &camera,
                ShaderProgram &shader,
                float aspectRatio,
                unsigned width,
                unsigned height);

    // Check if skybox is ready to render
    bool isValid() const { return m_vertexBuffer && m_indexBuffer && m_cubeMapSRV; }

    // Get cube map SRV for use in other rendering (e.g., reflections)
    ID3D11ShaderResourceView *getCubeMapSRV() const { return m_cubeMapSRV.Get(); }

private:
    void createCubeMesh(ID3D11Device *device);

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_cubeMapSRV;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerState;

    UINT m_indexCount = 0;
    UINT m_vertexStride = 0;
};
