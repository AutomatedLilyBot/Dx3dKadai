#include "Skybox.hpp"
#include <DirectXTex.h>
#include <cstdio>

using namespace DirectX;

// Skybox vertex structure (position only)
struct SkyboxVertex {
    XMFLOAT3 position;
};

bool Skybox::initialize(ID3D11Device *device) {
    if (!device) return false;

    createCubeMesh(device);

    // Create sampler state for cube map
    D3D11_SAMPLER_DESC sampDesc{};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    if (FAILED(device->CreateSamplerState(&sampDesc, m_samplerState.ReleaseAndGetAddressOf()))) {
        return false;
    }

    return true;
}

void Skybox::createCubeMesh(ID3D11Device *device) {
    // Create a cube with vertices at unit distance
    // Using inward-facing normals (by reversing winding order)
    const float s = 1.0f;
    SkyboxVertex vertices[] = {
        // Front face (Z+) - reversed winding
        {{-s, -s, s}}, {{s, -s, s}}, {{s, s, s}}, {{-s, s, s}},
        // Back face (Z-) - reversed winding
        {{s, -s, -s}}, {{-s, -s, -s}}, {{-s, s, -s}}, {{s, s, -s}},
        // Top face (Y+) - reversed winding
        {{-s, s, s}}, {{s, s, s}}, {{s, s, -s}}, {{-s, s, -s}},
        // Bottom face (Y-) - reversed winding
        {{-s, -s, -s}}, {{s, -s, -s}}, {{s, -s, s}}, {{-s, -s, s}},
        // Right face (X+) - reversed winding
        {{s, -s, s}}, {{s, -s, -s}}, {{s, s, -s}}, {{s, s, s}},
        // Left face (X-) - reversed winding
        {{-s, -s, -s}}, {{-s, -s, s}}, {{-s, s, s}}, {{-s, s, -s}},
    };

    // Indices for cube (reversed winding for inward facing)
    uint16_t indices[] = {
        // Front
        0, 1, 2, 0, 2, 3,
        // Back
        4, 5, 6, 4, 6, 7,
        // Top
        8, 9, 10, 8, 10, 11,
        // Bottom
        12, 13, 14, 12, 14, 15,
        // Right
        16, 17, 18, 16, 18, 19,
        // Left
        20, 21, 22, 20, 22, 23,
    };

    m_indexCount = _countof(indices);
    m_vertexStride = sizeof(SkyboxVertex);

    // Create vertex buffer
    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.ByteWidth = sizeof(vertices);
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData{};
    vbData.pSysMem = vertices;

    device->CreateBuffer(&vbDesc, &vbData, m_vertexBuffer.ReleaseAndGetAddressOf());

    // Create index buffer
    D3D11_BUFFER_DESC ibDesc{};
    ibDesc.ByteWidth = sizeof(indices);
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData{};
    ibData.pSysMem = indices;

    device->CreateBuffer(&ibDesc, &ibData, m_indexBuffer.ReleaseAndGetAddressOf());
}

bool Skybox::loadCubeMap(ID3D11Device *device,
                         const std::wstring &rightPath,
                         const std::wstring &leftPath,
                         const std::wstring &topPath,
                         const std::wstring &bottomPath,
                         const std::wstring &frontPath,
                         const std::wstring &backPath) {
    if (!device) return false;

    std::wstring paths[6] = {rightPath, leftPath, topPath, bottomPath, frontPath, backPath};

    // Load all 6 images
    DirectX::ScratchImage images[6];
    DirectX::TexMetadata metadata[6];

    for (int i = 0; i < 6; ++i) {
        HRESULT hr = LoadFromWICFile(paths[i].c_str(), WIC_FLAGS_NONE, &metadata[i], images[i]);
        if (FAILED(hr)) {
            wprintf(L"Failed to load skybox face %d from %s\n", i, paths[i].c_str());
            return false;
        }
    }

    // All faces should have the same dimensions
    size_t width = metadata[0].width;
    size_t height = metadata[0].height;

    // Create texture description for cube map
    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width = static_cast<UINT>(width);
    texDesc.Height = static_cast<UINT>(height);
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 6; // 6 faces for cube map
    texDesc.Format = metadata[0].format;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    // Prepare subresource data for all 6 faces
    D3D11_SUBRESOURCE_DATA initData[6];
    for (int i = 0; i < 6; ++i) {
        const DirectX::Image *img = images[i].GetImage(0, 0, 0);
        initData[i].pSysMem = img->pixels;
        initData[i].SysMemPitch = static_cast<UINT>(img->rowPitch);
        initData[i].SysMemSlicePitch = static_cast<UINT>(img->slicePitch);
    }

    // Create texture
    Microsoft::WRL::ComPtr<ID3D11Texture2D> cubeTexture;
    HRESULT hr = device->CreateTexture2D(&texDesc, initData, cubeTexture.GetAddressOf());
    if (FAILED(hr)) {
        wprintf(L"Failed to create cube map texture\n");
        return false;
    }

    // Create shader resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MipLevels = 1;
    srvDesc.TextureCube.MostDetailedMip = 0;

    hr = device->CreateShaderResourceView(cubeTexture.Get(), &srvDesc, m_cubeMapSRV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        wprintf(L"Failed to create cube map SRV\n");
        return false;
    }

    wprintf(L"Skybox cube map loaded successfully\n");
    return true;
}

bool Skybox::loadCubeMapFromDDS(ID3D11Device *device, const std::wstring &ddsPath) {
    if (!device) return false;

    DirectX::ScratchImage image;
    DirectX::TexMetadata metadata;

    HRESULT hr = LoadFromDDSFile(ddsPath.c_str(), DDS_FLAGS_NONE, &metadata, image);
    if (FAILED(hr)) {
        wprintf(L"Failed to load DDS cube map from %s\n", ddsPath.c_str());
        return false;
    }

    // Verify it's a cube map
    if (!metadata.IsCubemap()) {
        wprintf(L"DDS file is not a cube map: %s\n", ddsPath.c_str());
        return false;
    }

    // Create texture and SRV from DDS
    hr = CreateShaderResourceView(device, image.GetImages(), image.GetImageCount(),
                                  metadata, m_cubeMapSRV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        wprintf(L"Failed to create SRV from DDS cube map\n");
        return false;
    }

    wprintf(L"Skybox DDS cube map loaded successfully from %s\n", ddsPath.c_str());
    return true;
}

bool Skybox::createSolidColorCubeMap(ID3D11Device *device,
                                     unsigned char r,
                                     unsigned char g,
                                     unsigned char b,
                                     unsigned char a) {
    if (!device) return false;

    // Create a small cube map (e.g., 16x16 per face) with solid color
    const UINT texSize = 16;
    const UINT pixelCount = texSize * texSize;
    const UINT faceDataSize = pixelCount * 4; // RGBA

    // Create pixel data for one face
    std::vector<unsigned char> pixels(faceDataSize);
    for (UINT i = 0; i < pixelCount; ++i) {
        pixels[i * 4 + 0] = r;
        pixels[i * 4 + 1] = g;
        pixels[i * 4 + 2] = b;
        pixels[i * 4 + 3] = a;
    }

    // Create texture description for cube map
    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width = texSize;
    texDesc.Height = texSize;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 6; // 6 faces for cube map
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    // Prepare subresource data for all 6 faces (same color for all faces)
    D3D11_SUBRESOURCE_DATA initData[6];
    for (int i = 0; i < 6; ++i) {
        initData[i].pSysMem = pixels.data();
        initData[i].SysMemPitch = texSize * 4;
        initData[i].SysMemSlicePitch = faceDataSize;
    }

    // Create texture
    Microsoft::WRL::ComPtr<ID3D11Texture2D> cubeTexture;
    HRESULT hr = device->CreateTexture2D(&texDesc, initData, cubeTexture.GetAddressOf());
    if (FAILED(hr)) {
        wprintf(L"Failed to create solid color cube map texture\n");
        return false;
    }

    // Create shader resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MipLevels = 1;
    srvDesc.TextureCube.MostDetailedMip = 0;

    hr = device->CreateShaderResourceView(cubeTexture.Get(), &srvDesc, m_cubeMapSRV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        wprintf(L"Failed to create SRV for solid color cube map\n");
        return false;
    }

    wprintf(L"Solid color skybox cube map created successfully (RGBA: %d,%d,%d,%d)\n", r, g, b, a);
    return true;
}

void Skybox::render(ID3D11DeviceContext *context,
                    const Camera &camera,
                    ShaderProgram &shader,
                    float aspectRatio,
                    unsigned width,
                    unsigned height) {
    if (!isValid() || !context) return;

    // Bind shader
    shader.bind(context);

    // Set vertex buffer
    UINT stride = m_vertexStride;
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Get device (acquire once, release at end)
    ID3D11Device *device = nullptr;
    context->GetDevice(&device);
    if (!device) return;

    // Prepare transformation matrices
    // Remove translation from view matrix to keep skybox centered on camera
    XMMATRIX view = camera.getViewMatrix();
    // Zero out translation (4th column of view matrix)
    view.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

    XMMATRIX proj = camera.getProjectionMatrix(aspectRatio);
    XMMATRIX viewProj = view * proj;

    // Update constant buffer (shader expects ViewProj matrix at b0)
    struct SkyboxCB {
        XMFLOAT4X4 ViewProj;
    };
    SkyboxCB cbData;
    XMStoreFloat4x4(&cbData.ViewProj, XMMatrixTranspose(viewProj));

    // Create/update constant buffer
    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = sizeof(SkyboxCB);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    D3D11_SUBRESOURCE_DATA cbInitData{};
    cbInitData.pSysMem = &cbData;

    if (FAILED(device->CreateBuffer(&cbDesc, &cbInitData, constantBuffer.GetAddressOf()))) {
        device->Release();
        return;
    }

    // Bind constant buffer
    ID3D11Buffer *cbs[] = {constantBuffer.Get()};
    context->VSSetConstantBuffers(0, 1, cbs);

    // Bind cube map texture and sampler
    ID3D11ShaderResourceView *srvs[] = {m_cubeMapSRV.Get()};
    context->PSSetShaderResources(0, 1, srvs);

    ID3D11SamplerState *samplers[] = {m_samplerState.Get()};
    context->PSSetSamplers(0, 1, samplers);

    // Set render states
    // Disable depth write, use LESS_EQUAL depth test
    D3D11_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // No depth write
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL; // Allow depth = 1.0

    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthState;
    device->CreateDepthStencilState(&depthDesc, depthState.GetAddressOf());
    context->OMSetDepthStencilState(depthState.Get(), 0);

    // Disable culling (or use back-face culling depending on winding order)
    D3D11_RASTERIZER_DESC rastDesc{};
    rastDesc.FillMode = D3D11_FILL_SOLID;
    rastDesc.CullMode = D3D11_CULL_NONE; // No culling for skybox
    rastDesc.FrontCounterClockwise = FALSE;

    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rastState;
    device->CreateRasterizerState(&rastDesc, rastState.GetAddressOf());
    context->RSSetState(rastState.Get());

    // Draw skybox
    context->DrawIndexed(m_indexCount, 0, 0);

    // Cleanup
    ID3D11ShaderResourceView *nullSRV[] = {nullptr};
    context->PSSetShaderResources(0, 1, nullSRV);

    // Release device
    device->Release();
}
