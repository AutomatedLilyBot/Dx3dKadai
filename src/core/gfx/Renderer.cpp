#include "core/gfx/Renderer.hpp"
#include <string>
using namespace DirectX;

static const D3D11_INPUT_ELEMENT_DESC kLayout[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

static std::wstring ExeDir() {
    wchar_t buf[MAX_PATH]; GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring s(buf); auto p = s.find_last_of(L"\\/");
    return (p==std::wstring::npos) ? L"." : s.substr(0,p);
}

static bool CreateCB(ID3D11Device* dev, size_t bytes, ID3D11Buffer** out)
{
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = (UINT)((bytes + 15) & ~15);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    return SUCCEEDED(dev->CreateBuffer(&bd, nullptr, out));
}

bool Renderer::initialize(HWND hwnd, unsigned w, unsigned h, bool debug)
{
    if (!m_dev.initialize(hwnd, w, h, debug)) return false;

    std::wstring hlsl = ExeDir() + L"\\shader\\simple.hlsl";
    if (!m_shader.compileFromFile(m_dev.device(), hlsl, kLayout, _countof(kLayout),
                                  "VSMain", "PSMain"))
        return false;

    if (!m_cube.createCube(m_dev.device(), 1.0f)) return false;

    // Create constant buffers
    if (!CreateCB(m_dev.device(), sizeof(TransformCB), m_cbTransform.ReleaseAndGetAddressOf())) return false;
    if (!CreateCB(m_dev.device(), sizeof(LightCB), m_cbLight.ReleaseAndGetAddressOf())) return false;

    // Create default white texture (1x1 white)
    if (!m_defaultTexture.createSolidColor(m_dev.device(), 255, 255, 255, 255)) return false;

    // Create sampler state
    D3D11_SAMPLER_DESC sampDesc{};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(m_dev.device()->CreateSamplerState(&sampDesc, m_sampler.ReleaseAndGetAddressOf())))
        return false;

    // Set rasterizer state
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rs;
    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode=D3D11_FILL_SOLID;
    rd.CullMode=D3D11_CULL_BACK;
    rd.FrontCounterClockwise=TRUE;  // Counter-clockwise vertices are front-facing
    //rd.DepthClipEnable=TRUE;
    m_dev.device()->CreateRasterizerState(&rd, rs.GetAddressOf());
    m_dev.context()->RSSetState(rs.Get());

    // Set depth stencil state
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> dss;
    D3D11_DEPTH_STENCIL_DESC dsd{}; dsd.DepthEnable=TRUE;
    dsd.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;
    dsd.DepthFunc=D3D11_COMPARISON_LESS;
    m_dev.device()->CreateDepthStencilState(&dsd, dss.GetAddressOf());
    m_dev.context()->OMSetDepthStencilState(dss.Get(), 0);

    return true;
}

void Renderer::shutdown()
{
    m_sampler.Reset();
    m_cbLight.Reset();
    m_cbTransform.Reset();
    m_defaultTexture = Texture{};
    m_shader = ShaderProgram{};
    m_cube   = Mesh{};
    m_dev.shutdown();
}

void Renderer::beginFrame(float r,float g,float b,float a) { m_dev.clear(r,g,b,a); }
void Renderer::endFrame() { m_dev.present(); }

void Renderer::drawMesh(const Mesh& mesh, const DirectX::XMMATRIX& transform)
{
    using namespace DirectX;

    // Get view matrix from camera and calculate projection
    XMMATRIX V = m_camera.getViewMatrix();
    float aspect = (float)m_dev.width() / (float)m_dev.height();
    XMMATRIX P = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), aspect, 0.1f, 100.f);

    // Update transform constant buffer
    XMMATRIX WVP = transform * V * P;
    TransformCB cbTransform{};
    XMStoreFloat4x4(&cbTransform.World, transform);
    XMStoreFloat4x4(&cbTransform.WVP, WVP);
    m_dev.context()->UpdateSubresource(m_cbTransform.Get(), 0, nullptr, &cbTransform, 0, 0);

    // Update light constant buffer
    LightCB cbLight{};
    cbLight.lightDir = m_lightDir;
    cbLight.lightColor = m_lightColor;
    cbLight.ambientColor = m_ambientColor;
    m_dev.context()->UpdateSubresource(m_cbLight.Get(), 0, nullptr, &cbLight, 0, 0);

    // Bind shader
    m_shader.bind(m_dev.context());

    // Bind constant buffers
    ID3D11Buffer* cbs[] = { m_cbTransform.Get(), m_cbLight.Get() };
    m_dev.context()->VSSetConstantBuffers(0, 2, cbs);
    m_dev.context()->PSSetConstantBuffers(0, 2, cbs);

    // Bind texture and sampler
    ID3D11ShaderResourceView* srv = m_defaultTexture.srv();
    m_dev.context()->PSSetShaderResources(0, 1, &srv);
    ID3D11SamplerState* samp = m_sampler.Get();
    m_dev.context()->PSSetSamplers(0, 1, &samp);

    // Set render targets and viewport
    ID3D11RenderTargetView* rtv = m_dev.rtv();
    ID3D11DepthStencilView* dsv = m_dev.dsv();
    m_dev.context()->OMSetRenderTargets(1, &rtv, dsv);

    D3D11_VIEWPORT vp{0,0,(FLOAT)m_dev.width(),(FLOAT)m_dev.height(),0,1};
    m_dev.context()->RSSetViewports(1, &vp);

    // Draw mesh using its data
    UINT stride = mesh.stride();
    UINT offset = 0;
    ID3D11Buffer* vb = mesh.vertexBuffer();
    m_dev.context()->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    m_dev.context()->IASetIndexBuffer(mesh.indexBuffer(), DXGI_FORMAT_R16_UINT, 0);
    m_dev.context()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_dev.context()->DrawIndexed(mesh.indexCount(), 0, 0);
}
