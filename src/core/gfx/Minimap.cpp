#include "Minimap.hpp"
#include <cstdio>

using namespace DirectX;

bool Minimap::initialize(ID3D11Device *device, UINT width, UINT height) {
    if (!device) return false;

    m_width = width;
    m_height = height;

    if (!createRenderTarget(device)) {
        wprintf(L"Failed to create minimap render target\n");
        return false;
    }

    if (!createDepthStencil(device)) {
        wprintf(L"Failed to create minimap depth stencil\n");
        return false;
    }

    wprintf(L"Minimap initialized successfully (%dx%d)\n", m_width, m_height);
    return true;
}

bool Minimap::resize(ID3D11Device *device, UINT width, UINT height) {
    cleanup();
    return initialize(device, width, height);
}

void Minimap::cleanup() {
    m_rtv.Reset();
    m_srv.Reset();
    m_renderTexture.Reset();
    m_dsv.Reset();
    m_depthTexture.Reset();
}

bool Minimap::createRenderTarget(ID3D11Device *device) {
    // Create render texture (can be rendered to and sampled)
    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width = m_width;
    texDesc.Height = m_height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = 0;

    HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, m_renderTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        wprintf(L"Failed to create minimap texture (HRESULT: 0x%08X)\n", hr);
        return false;
    }

    // Create render target view
    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = texDesc.Format;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;

    hr = device->CreateRenderTargetView(m_renderTexture.Get(), &rtvDesc, m_rtv.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        wprintf(L"Failed to create minimap RTV (HRESULT: 0x%08X)\n", hr);
        return false;
    }

    // Create shader resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    hr = device->CreateShaderResourceView(m_renderTexture.Get(), &srvDesc, m_srv.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        wprintf(L"Failed to create minimap SRV (HRESULT: 0x%08X)\n", hr);
        return false;
    }

    return true;
}

bool Minimap::createDepthStencil(ID3D11Device *device) {
    // Create depth stencil texture
    D3D11_TEXTURE2D_DESC depthDesc{};
    depthDesc.Width = m_width;
    depthDesc.Height = m_height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depthDesc.CPUAccessFlags = 0;
    depthDesc.MiscFlags = 0;

    HRESULT hr = device->CreateTexture2D(&depthDesc, nullptr, m_depthTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        wprintf(L"Failed to create minimap depth texture (HRESULT: 0x%08X)\n", hr);
        return false;
    }

    // Create depth stencil view
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = depthDesc.Format;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    hr = device->CreateDepthStencilView(m_depthTexture.Get(), &dsvDesc, m_dsv.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        wprintf(L"Failed to create minimap DSV (HRESULT: 0x%08X)\n", hr);
        return false;
    }

    return true;
}

void Minimap::setupCamera(Camera &camera,
                          const DirectX::XMFLOAT3 &fieldCenter,
                          float fieldSize) {
    // Position camera above the field center
    XMFLOAT3 cameraPos = {
        fieldCenter.x,
        fieldCenter.y + fieldSize, // Height = field size
        fieldCenter.z
    };

    // Look down at the field center
    camera.setPosition(cameraPos);
    camera.setLookAt(fieldCenter);

    // Use orthographic projection
    camera.setOrthographic(true);
    camera.setOrthographicSize(fieldSize / 2.0f); // Half-size for full coverage
}

void Minimap::clear(ID3D11DeviceContext *context,
                    float r, float g, float b, float a) {
    if (!context || !m_rtv || !m_dsv) return;

    float clearColor[4] = {r, g, b, a};
    context->ClearRenderTargetView(m_rtv.Get(), clearColor);
    context->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}
