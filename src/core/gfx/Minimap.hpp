#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include "Camera.hpp"

// Minimap class: manages render-to-texture for minimap rendering
class Minimap {
public:
    Minimap() = default;

    ~Minimap() = default;

    // Initialize minimap with specified resolution
    bool initialize(ID3D11Device *device, UINT width, UINT height);

    // Resize minimap texture
    bool resize(ID3D11Device *device, UINT width, UINT height);

    // Get render target and depth stencil views for rendering
    ID3D11RenderTargetView *getRTV() const { return m_rtv.Get(); }
    ID3D11DepthStencilView *getDSV() const { return m_dsv.Get(); }

    // Get shader resource view for displaying minimap on screen
    ID3D11ShaderResourceView *getSRV() const { return m_srv.Get(); }

    // Setup camera for minimap rendering (orthographic top-down view)
    void setupCamera(Camera &camera,
                     const DirectX::XMFLOAT3 &fieldCenter,
                     float fieldSize);

    // Clear minimap render target
    void clear(ID3D11DeviceContext *context,
               float r, float g, float b, float a);

    // Get minimap dimensions
    UINT getWidth() const { return m_width; }
    UINT getHeight() const { return m_height; }

    // Check if minimap is valid
    bool isValid() const { return m_rtv && m_dsv && m_srv; }

private:
    void cleanup();

    bool createRenderTarget(ID3D11Device *device);

    bool createDepthStencil(ID3D11Device *device);

    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_renderTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_srv;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_depthTexture;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_dsv;

    UINT m_width = 256;
    UINT m_height = 256;
};
