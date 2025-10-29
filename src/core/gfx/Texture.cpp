#include "core/gfx/Texture.hpp"
#include <DirectXTex.h>
#include <wrl/client.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

bool Texture::loadFromFile(ID3D11Device* device, const std::wstring& filepath)
{
    // Determine file extension
    std::wstring ext = filepath.substr(filepath.find_last_of(L'.'));

    TexMetadata metadata;
    ScratchImage image;
    HRESULT hr;

    // Load based on file type
    if (ext == L".dds" || ext == L".DDS") {
        hr = LoadFromDDSFile(filepath.c_str(), DDS_FLAGS_NONE, &metadata, image);
    }
    else if (ext == L".tga" || ext == L".TGA") {
        hr = LoadFromTGAFile(filepath.c_str(), &metadata, image);
    }
    else if (ext == L".hdr" || ext == L".HDR") {
        hr = LoadFromHDRFile(filepath.c_str(), &metadata, image);
    }
    else {
        // WIC supports: jpg, png, bmp, gif, tiff, etc.
        hr = LoadFromWICFile(filepath.c_str(), WIC_FLAGS_NONE, &metadata, image);
    }

    if (FAILED(hr)) {
        return false;
    }

    // Create shader resource view
    hr = CreateShaderResourceView(device, image.GetImages(), image.GetImageCount(),
                                   metadata, m_srv.ReleaseAndGetAddressOf());

    return SUCCEEDED(hr);
}

bool Texture::createSolidColor(ID3D11Device* device, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    // Create a 1x1 texture with solid color
    unsigned char pixels[4] = { r, g, b, a };

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
