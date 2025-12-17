#include "Texture.hpp"
#include <DirectXTex.h>
#include <wrl/client.h>
#include <cwctype>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

bool Texture::loadFromFile(ID3D11Device *device, const std::wstring &filepath) {
    // Determine file extension
    std::wstring ext = filepath.substr(filepath.find_last_of(L'.'));

    TexMetadata metadata;
    ScratchImage image;
    HRESULT hr;

    // Load based on file type
    if (ext == L".dds" || ext == L".DDS") {
        hr = LoadFromDDSFile(filepath.c_str(), DDS_FLAGS_NONE, &metadata, image);
    } else if (ext == L".tga" || ext == L".TGA") {
        hr = LoadFromTGAFile(filepath.c_str(), &metadata, image);
    } else if (ext == L".hdr" || ext == L".HDR") {
        hr = LoadFromHDRFile(filepath.c_str(), &metadata, image);
    } else {
        // WIC supports: jpg, png, bmp, gif, tiff, etc.
        hr = LoadFromWICFile(filepath.c_str(), WIC_FLAGS_NONE, &metadata, image);
    }

    if (FAILED(hr)) {
        wprintf(L"Failed to load image file, HRESULT: 0x%08X\n", hr);
        return false;
    }

    // Create shader resource view
    hr = CreateShaderResourceView(device, image.GetImages(), image.GetImageCount(),
                                  metadata, m_srv.ReleaseAndGetAddressOf());

    if (FAILED(hr)) {
        wprintf(L"Failed to create SRV, HRESULT: 0x%08X\n", hr);
    }

    return SUCCEEDED(hr);
}

bool Texture::loadFromMemory(ID3D11Device *device, const void *data, size_t sizeInBytes, const char *formatHint) {
    if (!device || !data || sizeInBytes == 0) return false;

    TexMetadata metadata = {};
    ScratchImage image;
    HRESULT hr = E_FAIL;

    // Try according to format hint first
    if (formatHint) {
        std::string hint(formatHint);
        for (auto &c: hint) c = (char) std::towlower((wchar_t) c);
        if (hint == "dds") {
            hr = LoadFromDDSMemory(reinterpret_cast<const uint8_t *>(data), sizeInBytes, DDS_FLAGS_NONE, &metadata,
                                   image);
        } else if (hint == "tga") {
            // No direct TGA memory loader in DirectXTex, fall back to WIC
            hr = LoadFromWICMemory(reinterpret_cast<const uint8_t *>(data), sizeInBytes, WIC_FLAGS_NONE, &metadata,
                                   image);
        } else if (hint == "hdr") {
            hr = LoadFromHDRMemory(reinterpret_cast<const uint8_t *>(data), sizeInBytes, &metadata, image);
        } else {
            hr = LoadFromWICMemory(reinterpret_cast<const uint8_t *>(data), sizeInBytes, WIC_FLAGS_NONE, &metadata,
                                   image);
        }
    }

    // If failed or no hint, try common loaders
    if (FAILED(hr)) {
        hr = LoadFromDDSMemory(reinterpret_cast<const uint8_t *>(data), sizeInBytes, DDS_FLAGS_NONE, &metadata, image);
    }
    if (FAILED(hr)) {
        hr = LoadFromHDRMemory(reinterpret_cast<const uint8_t *>(data), sizeInBytes, &metadata, image);
    }
    if (FAILED(hr)) {
        hr = LoadFromWICMemory(reinterpret_cast<const uint8_t *>(data), sizeInBytes, WIC_FLAGS_NONE, &metadata, image);
    }

    if (FAILED(hr)) {
        return false;
    }

    hr = CreateShaderResourceView(device, image.GetImages(), image.GetImageCount(), metadata,
                                  m_srv.ReleaseAndGetAddressOf());
    return SUCCEEDED(hr);
}

bool Texture::createFromRGBA(ID3D11Device *device, const void *rgbaPixels, uint32_t width, uint32_t height,
                             uint32_t rowPitch) {
    if (!device || !rgbaPixels || width == 0 || height == 0) return false;

    if (rowPitch == 0) rowPitch = width * 4u;

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = rgbaPixels;
    initData.SysMemPitch = rowPitch;

    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = device->CreateTexture2D(&texDesc, &initData, texture.GetAddressOf());
    if (FAILED(hr)) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    hr = device->CreateShaderResourceView(texture.Get(), &srvDesc, m_srv.ReleaseAndGetAddressOf());
    return SUCCEEDED(hr);
}

bool Texture::createSolidColor(ID3D11Device *device, unsigned char r, unsigned char g, unsigned char b,
                               unsigned char a) {
    // Create a 1x1 texture with solid color
    unsigned char pixels[4] = {r, g, b, a};

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = 1;
    texDesc.Height = 1;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pixels;
    initData.SysMemPitch = 4;

    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = device->CreateTexture2D(&texDesc, &initData, texture.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    // Create shader resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    hr = device->CreateShaderResourceView(texture.Get(), &srvDesc, m_srv.ReleaseAndGetAddressOf());

    return SUCCEEDED(hr);
}
