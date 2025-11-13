#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <cstdint>

class Texture {
public:
    Texture() = default;
    ~Texture() = default;

    // Load texture from file using DirectXTex
    bool loadFromFile(ID3D11Device* device, const std::wstring& filepath);

    // Load texture from an in-memory buffer (DDS/WIC formats). Optional formatHint like "png", "jpg", "dds".
    bool loadFromMemory(ID3D11Device* device, const void* data, size_t sizeInBytes, const char* formatHint = nullptr);

    // Create from raw RGBA8 pixel data (width*height*4 bytes)
    bool createFromRGBA(ID3D11Device* device, const void* rgbaPixels, uint32_t width, uint32_t height, uint32_t rowPitch = 0);

    // Create a simple solid color texture (e.g., white texture for default)
    bool createSolidColor(ID3D11Device* device, unsigned char r, unsigned char g, unsigned char b, unsigned char a);

    // Get shader resource view
    ID3D11ShaderResourceView* srv() const { return m_srv.Get(); }

    // Check if texture is loaded
    bool isValid() const { return m_srv != nullptr; }

private:
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_srv;
};
