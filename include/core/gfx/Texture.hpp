#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <string>

class Texture {
public:
    Texture() = default;
    ~Texture() = default;

    // Load texture from file using DirectXTex
    bool loadFromFile(ID3D11Device* device, const std::wstring& filepath);

    // Create a simple solid color texture (e.g., white texture for default)
    bool createSolidColor(ID3D11Device* device, unsigned char r, unsigned char g, unsigned char b, unsigned char a);

    // Get shader resource view
    ID3D11ShaderResourceView* srv() const { return m_srv.Get(); }

    // Check if texture is loaded
    bool isValid() const { return m_srv != nullptr; }

private:
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_srv;
};
