//
// Created by zyzyz on 2025/10/22.
//


#include "core/gfx/Renderer.hpp"
#include <string>
#include "core/gfx/ShaderProgram.hpp"

using namespace DirectX;

// 放在 Renderer.cpp 顶部或匿名命名空间
static bool CreateConstantBuffer(ID3D11Device *dev, size_t bytes, ID3D11Buffer **out) {
    // D3D11 要求常量缓冲大小按 16 字节对齐
    UINT byteWidth = (UINT) ((bytes + 15) & ~15);

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = byteWidth;
    bd.Usage = D3D11_USAGE_DEFAULT; // 用 UpdateSubresource 更新
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;
    bd.StructureByteStride = 0;

    return SUCCEEDED(dev->CreateBuffer(&bd, nullptr, out));
}



static const D3D11_INPUT_ELEMENT_DESC kLayout[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
};


bool Renderer::initialize(HWND hwnd, unsigned width, unsigned height, bool debug) {
    if (!m_dev.initialize(hwnd, width, height, debug)) return false;

    // 1) 计算 HLSL 路径：exe 同级的 "shaders/Phong.hlsl"
    auto GetExeDir = []() -> std::wstring {
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        std::wstring path(buf);
        size_t pos = path.find_last_of(L"\\/");
        return (pos == std::wstring::npos) ? L"." : path.substr(0, pos);
    };
    const std::wstring hlslPath = GetExeDir() + L"\\shader\\phong.hlsl";

    // 2) 编译
    if (!m_phong.compileFromFile(m_dev.device(), hlslPath, kLayout, _countof(kLayout)))
        return false;


    if (!CreateConstantBuffer(m_dev.device(), sizeof(FrameCB), m_cbFrame.ReleaseAndGetAddressOf())) return false;
    if (!CreateConstantBuffer(m_dev.device(), sizeof(ObjectCB), m_cbObject.ReleaseAndGetAddressOf())) return false;
    if (!CreateConstantBuffer(m_dev.device(), sizeof(MaterialCB), m_cbMaterial.ReleaseAndGetAddressOf())) return false;
    if (!CreateConstantBuffer(m_dev.device(), sizeof(PointLightsCB), m_cbPoint.ReleaseAndGetAddressOf())) return false;


    return true;  // ← 这是关键！必须返回 true
}

void Renderer::shutdown() {
    m_sampler.Reset();
    m_cbPoint.Reset();
    m_cbMaterial.Reset();
    m_cbObject.Reset();
    m_cbFrame.Reset();
    m_quad = Mesh{};
    m_phong = ShaderProgram{};
    m_dev.shutdown();
}


void Renderer::beginFrame(float r, float g, float b, float a) {
    m_dev.clear(r, g, b, a);
}


void Renderer::endFrame() {
    m_dev.present();
}


void Renderer::drawSprite(const SpriteDesc3D &d) {
    // --- Build matrices ---
    const float aspect = (float) m_dev.width() / (float) m_dev.height();
    XMMATRIX view = XMMatrixLookAtLH(XMLoadFloat3(&m_cameraPos), XMVectorSet(0, 0, 0, 1), XMVectorSet(0, 1, 0, 0));
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), aspect, 0.1f, 100.0f);


    XMMATRIX S = XMMatrixScaling(d.size.x, d.size.y, 1.0f);
    XMMATRIX R = XMMatrixRotationY(d.rotationY);
    XMMATRIX T = XMMatrixTranslation(d.position.x, d.position.y, d.position.z);
    XMMATRIX W = S * R * T;


    // Normal matrix = transpose(inverse(world)) for 3x3 part -> store in 4x4
    XMMATRIX WN = XMMatrixTranspose(XMMatrixInverse(nullptr, W));


    // --- Update CBs ---
    FrameCB f{};
    XMStoreFloat4x4(&f.view, view);
    XMStoreFloat4x4(&f.proj, proj);
    f.camPos = m_cameraPos;
    f.ambient = XMFLOAT3(0.05f, 0.05f, 0.08f);
    // f._pad1 is already zero-initialized
    f.dirDir = XMFLOAT3(-0.4f, -1.0f, -0.2f);
    f.dirIntensity = 1.0f;
    f.dirColor = XMFLOAT3(1.0f, 0.96f, 0.9f);
    // f._pad2 is already zero-initialized
    f.pointCount = 1u;
    // f._pad3 is already zero-initialized


    ObjectCB o{};
    XMStoreFloat4x4(&o.world, W);
    XMStoreFloat4x4(&o.worldNormal, WN);


    MaterialCB m{};
    m.albedoFactor = d.albedo;
    m.useTex = 0.0f;
    m.specColor = XMFLOAT3(1, 1, 1);
    m.specPower = 64.0f;
    m.emissive = XMFLOAT3(0, 0, 0);


    PointLightsCB p{};
    p.lights[0] = {XMFLOAT3(0.0f, 1.5f, -2.0f), 2.0f, XMFLOAT3(0.8f, 0.9f, 1.0f), 6.0f};


    auto ctx = m_dev.context();
    // Add validation

    ctx->UpdateSubresource(m_cbFrame.Get(), 0, nullptr, &f, 0, 0);
    ctx->UpdateSubresource(m_cbObject.Get(), 0, nullptr, &o, 0, 0);
    ctx->UpdateSubresource(m_cbMaterial.Get(), 0, nullptr, &m, 0, 0);
    ctx->UpdateSubresource(m_cbPoint.Get(), 0, nullptr, &p, 0, 0);


    // --- Bind pipeline ---
    m_phong.bind(ctx);
    ctx->VSSetConstantBuffers(0, 1, m_cbFrame.GetAddressOf());
    ctx->VSSetConstantBuffers(1, 1, m_cbObject.GetAddressOf());
    ctx->PSSetConstantBuffers(0, 1, m_cbFrame.GetAddressOf());
    ctx->PSSetConstantBuffers(1, 1, m_cbObject.GetAddressOf());
    ctx->PSSetConstantBuffers(2, 1, m_cbMaterial.GetAddressOf());
    ctx->PSSetConstantBuffers(3, 1, m_cbPoint.GetAddressOf());
    ID3D11SamplerState *samp = m_sampler.Get();
    ctx->PSSetSamplers(0, 1, &samp);


    // Draw quad
    m_quad.draw(ctx);
}
