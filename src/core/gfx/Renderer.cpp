#include "Renderer.hpp"
#undef min
#undef max
#include <cmath>
#include <string>
#include <algorithm>
#include <cstring>
#include "../render/Drawable.hpp"
#include <vector>
using namespace DirectX;

static const D3D11_INPUT_ELEMENT_DESC kLayout[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

static const D3D11_INPUT_ELEMENT_DESC kUiLayout[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

static std::wstring ExeDir() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring s(buf);
    auto p = s.find_last_of(L"\\/");
    return (p == std::wstring::npos) ? L"." : s.substr(0, p);
}

static bool CreateCB(ID3D11Device *dev, size_t bytes, ID3D11Buffer **out) {
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = (UINT) ((bytes + 15) & ~15);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    return SUCCEEDED(dev->CreateBuffer(&bd, nullptr, out));
}

bool Renderer::initialize(HWND hwnd, unsigned w, unsigned h, bool debug) {
    if (!m_dev.initialize(hwnd, w, h, debug)) return false;

    std::wstring hlsl = ExeDir() + L"\\shader\\simple.hlsl";
    wprintf(L"Compiling shader from: %s\n", hlsl.c_str());
    if (!m_shader.compileFromFile(m_dev.device(), hlsl, kLayout, _countof(kLayout),
                                  "VSMain", "PSMain")) {
        wprintf(L"ERROR: Failed to compile shader!\n");
        return false;
    }
    wprintf(L"Shader compiled successfully\n");

    std::wstring uiHlsl = ExeDir() + L"\\shader\\ui.hlsl";
    wprintf(L"Compiling UI shader from: %s\n", uiHlsl.c_str());
    if (!m_uiShader.compileFromFile(m_dev.device(), uiHlsl, kUiLayout, _countof(kUiLayout),
                                    "VSMain", "PSMain")) {
        wprintf(L"ERROR: Failed to compile UI shader!\n");
        return false;
    }

    if (!m_cube.createCube(m_dev.device(), 1.0f)) return false;

    // Create constant buffers
    if (!CreateCB(m_dev.device(), sizeof(TransformCB), m_cbTransform.ReleaseAndGetAddressOf())) return false;
    if (!CreateCB(m_dev.device(), sizeof(LightCB), m_cbLight.ReleaseAndGetAddressOf())) return false;
    if (!CreateCB(m_dev.device(), sizeof(MaterialCB), m_cbMaterial.ReleaseAndGetAddressOf())) return false;
    if (!CreateCB(m_dev.device(), sizeof(UiCB), m_cbUi.ReleaseAndGetAddressOf())) return false;

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
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_BACK;
    rd.FrontCounterClockwise = FALSE; // Counter-clockwise vertices are front-facing
    //rd.DepthClipEnable=TRUE;
    m_dev.device()->CreateRasterizerState(&rd, rs.GetAddressOf());
    m_dev.context()->RSSetState(rs.Get());

    // Set depth stencil state
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> dss;
    D3D11_DEPTH_STENCIL_DESC dsd{};
    dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsd.DepthFunc = D3D11_COMPARISON_LESS;
    m_dev.device()->CreateDepthStencilState(&dsd, dss.GetAddressOf());
    m_depthState = dss;
    m_dev.context()->OMSetDepthStencilState(dss.Get(), 0);

    D3D11_DEPTH_STENCIL_DESC uiDsd{};
    uiDsd.DepthEnable = FALSE;
    uiDsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    uiDsd.DepthFunc = D3D11_COMPARISON_ALWAYS;
    m_dev.device()->CreateDepthStencilState(&uiDsd, m_uiDepthState.ReleaseAndGetAddressOf());

    // Create depth-no-write state (for transparent objects: test but don't write)
    D3D11_DEPTH_STENCIL_DESC depthNoWriteDsd{};
    depthNoWriteDsd.DepthEnable = TRUE;
    depthNoWriteDsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // 不写入深度
    depthNoWriteDsd.DepthFunc = D3D11_COMPARISON_LESS; // 但仍然测试深度
    m_dev.device()->CreateDepthStencilState(&depthNoWriteDsd, m_depthNoWriteState.ReleaseAndGetAddressOf());

    // Create alpha blend state (for transparent objects)
    D3D11_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    m_dev.device()->CreateBlendState(&blendDesc, m_alphaBlendState.ReleaseAndGetAddressOf());

    // Create no-cull rasterizer state (for billboards: render both sides)
    D3D11_RASTERIZER_DESC noCullRd{};
    noCullRd.FillMode = D3D11_FILL_SOLID;
    noCullRd.CullMode = D3D11_CULL_NONE; // 双面渲染
    noCullRd.FrontCounterClockwise = FALSE;
    m_dev.device()->CreateRasterizerState(&noCullRd, m_noCullRasterizerState.ReleaseAndGetAddressOf());

    D3D11_BUFFER_DESC uiVb{};
    uiVb.ByteWidth = sizeof(DirectX::XMFLOAT4) * 4;
    uiVb.Usage = D3D11_USAGE_DYNAMIC;
    uiVb.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    uiVb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    m_dev.device()->CreateBuffer(&uiVb, nullptr, m_uiVB.ReleaseAndGetAddressOf());

    return true;
}

void Renderer::shutdown() {
    m_sampler.Reset();
    m_cbMaterial.Reset();
    m_cbLight.Reset();
    m_cbTransform.Reset();
    m_cbUi.Reset();
    m_uiVB.Reset();
    m_uiDepthState.Reset();
    m_depthState.Reset();
    m_depthNoWriteState.Reset();
    m_alphaBlendState.Reset();
    m_noCullRasterizerState.Reset();
    m_defaultTexture = Texture{};
    m_shader = ShaderProgram{};
    m_uiShader = ShaderProgram{};
    m_cube = Mesh{};
    m_dev.shutdown();
}

void Renderer::beginFrame(float r, float g, float b, float a) { m_dev.clear(r, g, b, a); }
void Renderer::endFrame() { m_dev.present(); }

void Renderer::drawModel(const Model &model, const DirectX::XMMATRIX &modelTransform,
                         const DirectX::XMFLOAT4 *baseColorFactor) {
    // Iterate through draw items and render each sub-mesh with its material
    for (const auto &di: model.drawItems) {
        if (di.meshIndex >= model.meshes.size()) continue;
        const auto &mg = model.meshes[di.meshIndex];
        const Texture *tex = nullptr;
        if (mg.materialIndex >= 0 && static_cast<size_t>(mg.materialIndex) < model.materials.size()) {
            tex = &model.materials[mg.materialIndex].diffuse;
        }
        // Combine model transform with node-global transform
        DirectX::XMMATRIX nodeM = DirectX::XMLoadFloat4x4(&di.nodeGlobal);
        DirectX::XMMATRIX world = modelTransform;
        //DirectX::XMMATRIX world = modelTransform;
        drawMesh(mg.mesh, world, tex ? *tex : m_defaultTexture, baseColorFactor);
    }
}

void Renderer::drawCube(const DirectX::XMMATRIX &worldTransform) {
    // Use default texture and cube mesh for a quick demo draw
    drawMesh(m_cube, worldTransform, m_defaultTexture);
}

void Renderer::drawUiQuad(float x, float y, float width, float height,
                          ID3D11ShaderResourceView *texture,
                          const DirectX::XMFLOAT4 &tint,
                          float emissive,
                          float uvOffsetX,
                          float uvOffsetY) {
    if (!m_uiShader.vs() || !m_uiVB) return;

    const float left = x * 2.0f - 1.0f;
    const float right = (x + width) * 2.0f - 1.0f;
    const float top = 1.0f - y * 2.0f;
    const float bottom = 1.0f - (y + height) * 2.0f;

    struct UiVertex {
        DirectX::XMFLOAT2 pos;
        DirectX::XMFLOAT2 uv;
    };
    UiVertex verts[4] = {
        {{left, bottom}, {0.0f, 1.0f}},
        {{left, top}, {0.0f, 0.0f}},
        {{right, bottom}, {1.0f, 1.0f}},
        {{right, top}, {1.0f, 0.0f}},
    };

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(m_dev.context()->Map(m_uiVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        memcpy(mapped.pData, verts, sizeof(verts));
        m_dev.context()->Unmap(m_uiVB.Get(), 0);
    }

    UiCB cb{tint, emissive, uvOffsetX, uvOffsetY, 0.0f};
    m_dev.context()->UpdateSubresource(m_cbUi.Get(), 0, nullptr, &cb, 0, 0);

    UINT stride = sizeof(UiVertex);
    UINT offset = 0;
    m_dev.context()->IASetInputLayout(m_uiShader.inputLayout());
    m_dev.context()->IASetVertexBuffers(0, 1, m_uiVB.GetAddressOf(), &stride, &offset);
    m_dev.context()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    m_dev.context()->VSSetShader(m_uiShader.vs(), nullptr, 0);
    m_dev.context()->PSSetShader(m_uiShader.ps(), nullptr, 0);
    ID3D11Buffer *cbuffers[] = {m_cbUi.Get()};
    m_dev.context()->VSSetConstantBuffers(0, 1, cbuffers);
    m_dev.context()->PSSetConstantBuffers(0, 1, cbuffers);

    ID3D11ShaderResourceView *srv = texture ? texture : m_defaultTexture.srv();
    m_dev.context()->PSSetShaderResources(0, 1, &srv);
    m_dev.context()->PSSetSamplers(0, 1, m_sampler.GetAddressOf());

    m_dev.context()->OMSetDepthStencilState(m_uiDepthState.Get(), 0);

    // Enable alpha blending for UI transparency
    if (m_alphaBlendState) {
        float blendFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        m_dev.context()->OMSetBlendState(m_alphaBlendState.Get(), blendFactor, 0xFFFFFFFF);
    }

    m_dev.context()->Draw(4, 0);

    // Restore blend state and depth state
    m_dev.context()->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    if (m_depthState) {
        m_dev.context()->OMSetDepthStencilState(m_depthState.Get(), 0);
    }
}

void Renderer::draw(const IDrawable &drawable) {
    const Model *m = drawable.model();
    if (m) {
        const Material *mat = drawable.material();
        const DirectX::XMFLOAT4 *baseColor = mat ? &mat->baseColorFactor : nullptr;
        drawModel(*m, drawable.world(), baseColor);
    }
}

// ---- Debug line rendering for colliders ----
void Renderer::drawColliderWire(const ColliderBase &col) {
    if (!col.debugEnabled()) return;

    struct DebugLineVertex {
        XMFLOAT3 pos;
        XMFLOAT3 normal;
        XMFLOAT4 color;
        XMFLOAT2 uv;
    };
    std::vector<DebugLineVertex> vtx;
    vtx.reserve(1024);

    auto addLine = [&](const XMFLOAT3 &a, const XMFLOAT3 &b, const XMFLOAT4 &color) {
        DebugLineVertex va{a, XMFLOAT3{0, 0, 1}, color, XMFLOAT2{0, 0}};
        DebugLineVertex vb{b, XMFLOAT3{0, 0, 1}, color, XMFLOAT2{0, 0}};
        vtx.push_back(va);
        vtx.push_back(vb);
    };

    XMFLOAT4 c = col.debugColor();
    switch (col.kind()) {
        case ColliderType::Sphere: {
            auto &s = static_cast<const SphereCollider &>(col);
            XMFLOAT3 center = s.centerWorld();
            float r = s.radiusWorld();
            const int N = 32;
            auto circle = [&](int axA, int axB) {
                XMFLOAT3 prev{};
                bool hasPrev = false;
                for (int i = 0; i <= N; i++) {
                    float t = (float) i / (float) N * XM_2PI;
                    float ct = std::cos(t), st = std::sin(t);
                    XMFLOAT3 p = center;
                    float coords[3] = {center.x, center.y, center.z};
                    coords[axA] += r * ct;
                    coords[axB] += r * st;
                    p.x = coords[0];
                    p.y = coords[1];
                    p.z = coords[2];
                    if (hasPrev) addLine(prev, p, c);
                    prev = p;
                    hasPrev = true;
                }
            };
            circle(0, 1); // XY
            circle(1, 2); // YZ
            circle(0, 2); // XZ
        }
        break;
        case ColliderType::Obb: {
            auto &o = static_cast<const ObbCollider &>(col);
            XMFLOAT3 axes[3];
            o.axesWorld(axes);
            XMFLOAT3 he = o.halfExtentsWorld();
            XMFLOAT3 c0 = o.centerWorld();
            // 8 corners
            XMFLOAT3 corners[8];
            int idx = 0;
            for (int sx = -1; sx <= 1; sx += 2) {
                for (int sy = -1; sy <= 1; sy += 2) {
                    for (int sz = -1; sz <= 1; sz += 2) {
                        XMFLOAT3 p = c0;
                        p.x += axes[0].x * he.x * sx + axes[1].x * he.y * sy + axes[2].x * he.z * sz;
                        p.y += axes[0].y * he.x * sx + axes[1].y * he.y * sy + axes[2].y * he.z * sz;
                        p.z += axes[0].z * he.x * sx + axes[1].z * he.y * sy + axes[2].z * he.z * sz;
                        corners[idx++] = p;
                    }
                }
            }
            auto corner = [&](int sx, int sy, int sz)-> XMFLOAT3 & {
                int ix = (sx < 0 ? 0 : 1), iy = (sy < 0 ? 0 : 1), iz = (sz < 0 ? 0 : 1);
                int id = (ix << 2) | (iy << 1) | iz;
                return corners[id];
            };
            // 12 edges
            addLine(corner(-1, -1, -1), corner(1, -1, -1), c);
            addLine(corner(-1, 1, -1), corner(1, 1, -1), c);
            addLine(corner(-1, -1, 1), corner(1, -1, 1), c);
            addLine(corner(-1, 1, 1), corner(1, 1, 1), c);
            addLine(corner(-1, -1, -1), corner(-1, 1, -1), c);
            addLine(corner(1, -1, -1), corner(1, 1, -1), c);
            addLine(corner(-1, -1, 1), corner(-1, 1, 1), c);
            addLine(corner(1, -1, 1), corner(1, 1, 1), c);
            addLine(corner(-1, -1, -1), corner(-1, -1, 1), c);
            addLine(corner(1, -1, -1), corner(1, -1, 1), c);
            addLine(corner(-1, 1, -1), corner(-1, 1, 1), c);
            addLine(corner(1, 1, -1), corner(1, 1, 1), c);
        }
        break;
        case ColliderType::Capsule: {
            auto &cp = static_cast<const CapsuleCollider &>(col);
            auto seg = cp.segmentWorld();
            float r = cp.radiusWorld();
            XMVECTOR P0 = XMLoadFloat3(&seg.first);
            XMVECTOR P1 = XMLoadFloat3(&seg.second);
            XMVECTOR axis = XMVectorSubtract(P1, P0);
            XMVECTOR axN = XMVector3Normalize(axis);
            // build an orthonormal basis (u,v,w=axN)
            XMVECTOR tmp = XMVectorSet(0, 1, 0, 0);
            if (std::fabs(XMVectorGetX(XMVector3Dot(tmp, axN))) > 0.99f) tmp = XMVectorSet(1, 0, 0, 0);
            XMVECTOR u = XMVector3Normalize(XMVector3Cross(axN, tmp));
            XMVECTOR v = XMVector3Cross(axN, u);
            auto toFloat3 = [](FXMVECTOR q) {
                XMFLOAT3 o;
                XMStoreFloat3(&o, q);
                return o;
            };
            const int N = 24;
            // two end rings
            XMFLOAT3 prev0{}, prev1{};
            bool hp0 = false, hp1 = false;
            for (int i = 0; i <= N; i++) {
                float t = (float) i / (float) N * XM_2PI;
                float ct = std::cos(t), st = std::sin(t);
                XMVECTOR off = XMVectorAdd(XMVectorScale(u, r * ct), XMVectorScale(v, r * st));
                XMVECTOR q0 = XMVectorAdd(P0, off);
                XMVECTOR q1 = XMVectorAdd(P1, off);
                XMFLOAT3 f0 = toFloat3(q0);
                XMFLOAT3 f1 = toFloat3(q1);
                if (hp0) addLine(prev0, f0, c);
                if (hp1) addLine(prev1, f1, c);
                prev0 = f0;
                prev1 = f1;
                hp0 = hp1 = true;
                // side lines sparsely
                if (i % 6 == 0) addLine(f0, f1, c);
            }
            // axis line
            addLine(seg.first, seg.second, c);
        }
        break;
    }

    if (vtx.empty()) return;

    // Ensure dynamic VB is large enough
    UINT bytes = (UINT) (vtx.size() * sizeof(DebugLineVertex));
    if (!m_debugVB || true) {
        // Recreate buffer each draw for simplicity (small data for debug)
        m_debugVB.Reset();
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = bytes;
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        D3D11_SUBRESOURCE_DATA init{vtx.data(), 0, 0};
        m_dev.device()->CreateBuffer(&bd, &init, m_debugVB.ReleaseAndGetAddressOf());
    } else {
        D3D11_MAPPED_SUBRESOURCE ms{};
        if (SUCCEEDED(m_dev.context()->Map(m_debugVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
            memcpy(ms.pData, vtx.data(), bytes);
            m_dev.context()->Unmap(m_debugVB.Get(), 0);
        }
    }

    // Set transform for lines (world = identity; vertices already in world space)
    XMMATRIX I = XMMatrixIdentity();
    const Camera &cam = getActiveCamera();
    XMMATRIX V = cam.getViewMatrix();
    float aspect = (float) m_dev.width() / (float) m_dev.height();
    XMMATRIX P = cam.getProjectionMatrix(aspect);
    TransformCB cb{};
    XMStoreFloat4x4(&cb.World, I);
    XMStoreFloat4x4(&cb.WVP, I * V * P);
    m_dev.context()->UpdateSubresource(m_cbTransform.Get(), 0, nullptr, &cb, 0, 0);

    // Light CB unchanged
    LightCB cbLight{};
    cbLight.lightDir = m_lightDir;
    cbLight.lightColor = m_lightColor;
    cbLight.ambientColor = m_ambientColor;
    m_dev.context()->UpdateSubresource(m_cbLight.Get(), 0, nullptr, &cbLight, 0, 0);

    // Material CB for debug lines (white)
    MaterialCB cbMaterial{};
    cbMaterial.baseColorFactor = XMFLOAT4(1, 1, 1, 1);
    m_dev.context()->UpdateSubresource(m_cbMaterial.Get(), 0, nullptr, &cbMaterial, 0, 0);

    // Bind shader and CBs
    m_shader.bind(m_dev.context());
    ID3D11Buffer *cbs[] = {m_cbTransform.Get(), m_cbLight.Get(), m_cbMaterial.Get()};
    m_dev.context()->VSSetConstantBuffers(0, 3, cbs);
    m_dev.context()->PSSetConstantBuffers(0, 3, cbs);

    // Bind default white texture and sampler
    ID3D11ShaderResourceView *srv = m_defaultTexture.srv();
    m_dev.context()->PSSetShaderResources(0, 1, &srv);
    ID3D11SamplerState *samp = m_sampler.Get();
    m_dev.context()->PSSetSamplers(0, 1, &samp);

    // Set RTs and viewport
    ID3D11RenderTargetView *rtv = m_dev.rtv();
    ID3D11DepthStencilView *dsv = m_dev.dsv();
    m_dev.context()->OMSetRenderTargets(1, &rtv, dsv);
    D3D11_VIEWPORT vp{0, 0, (FLOAT) m_dev.width(), (FLOAT) m_dev.height(), 0, 1};
    m_dev.context()->RSSetViewports(1, &vp);

    // IA setup and draw
    UINT stride = sizeof(DebugLineVertex), offset = 0;
    ID3D11Buffer *vb = m_debugVB.Get();
    m_dev.context()->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    m_dev.context()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    m_dev.context()->Draw((UINT) vtx.size(), 0);

    ID3D11ShaderResourceView *nullSRV[1] = {nullptr};
    m_dev.context()->PSSetShaderResources(0, 1, nullSRV);
}

void Renderer::drawCollidersWire(const std::vector<ColliderBase *> &cols) {
    for (auto *c: cols) if (c) drawColliderWire(*c);
}

// ---- Debug draw for contact info (OverlapResult) ----
void Renderer::drawContactGizmo(const OverlapResult &contact, const XMFLOAT4 &color, float scale) {
    using namespace DirectX;
    if (!contact.intersects) return;

    struct DebugLineVertex {
        XMFLOAT3 pos;
        XMFLOAT3 normal;
        XMFLOAT4 color;
        XMFLOAT2 uv;
    };
    std::vector<DebugLineVertex> vtx;
    vtx.reserve(128);
    auto addLine = [&](const XMFLOAT3 &a, const XMFLOAT3 &b, const XMFLOAT4 &c) {
        vtx.push_back(DebugLineVertex{a, XMFLOAT3{0, 0, 1}, c, XMFLOAT2{0, 0}});
        vtx.push_back(DebugLineVertex{b, XMFLOAT3{0, 0, 1}, c, XMFLOAT2{0, 0}});
    };

    const XMFLOAT3 &A = contact.pointOnA;
    const XMFLOAT3 &B = contact.pointOnB;
    const XMFLOAT3 &N = contact.normal; // assumed A->B

    // Small crosses at A and B
    auto addCross = [&](const XMFLOAT3 &p, float s) {
        XMFLOAT3 dx{s, 0, 0}, dy{0, s, 0}, dz{0, 0, s};
        addLine(XMFLOAT3{p.x - s, p.y, p.z}, XMFLOAT3{p.x + s, p.y, p.z}, color);
        addLine(XMFLOAT3{p.x, p.y - s, p.z}, XMFLOAT3{p.x, p.y + s, p.z}, color);
        addLine(XMFLOAT3{p.x, p.y, p.z - s}, XMFLOAT3{p.x, p.y, p.z + s}, color);
    };
    float crossSize = std::max(0.025f, 0.04f * scale);
    addCross(A, crossSize);
    addCross(B, crossSize);

    // Line between A and B
    addLine(A, B, color);

    // Contact normal arrow, from midpoint towards N
    XMFLOAT3 mid{(A.x + B.x) * 0.5f, (A.y + B.y) * 0.5f, (A.z + B.z) * 0.5f};
    float len = std::max(contact.penetration, 0.15f) * scale;
    XMFLOAT3 tip{mid.x + N.x * len, mid.y + N.y * len, mid.z + N.z * len};
    addLine(mid, tip, color);
    // Arrow wings
    // Build orthonormal basis for N
    XMVECTOR nV = XMVector3Normalize(XMLoadFloat3(&N));
    XMVECTOR tmp = XMVectorSet(0, 1, 0, 0);
    if (std::fabs(XMVectorGetX(XMVector3Dot(nV, tmp))) > 0.99f) tmp = XMVectorSet(1, 0, 0, 0);
    XMVECTOR u = XMVector3Normalize(XMVector3Cross(nV, tmp));
    XMVECTOR v = XMVector3Cross(nV, u);
    float wingLen = std::max(0.06f, 0.1f * scale);
    XMVECTOR tipV = XMLoadFloat3(&tip);
    XMVECTOR back = XMVectorSubtract(tipV, XMVectorScale(nV, wingLen));
    XMFLOAT3 wing1, wing2;
    XMStoreFloat3(&wing1, XMVectorAdd(back, XMVectorScale(u, wingLen * 0.6f)));
    XMStoreFloat3(&wing2, XMVectorAdd(back, XMVectorScale(v, wingLen * 0.6f)));
    addLine(tip, wing1, color);
    addLine(tip, wing2, color);

    if (vtx.empty()) return;

    // Create/update dynamic VB (recreate for simplicity)
    UINT bytes = (UINT) (vtx.size() * sizeof(DebugLineVertex));
    m_debugVB.Reset();
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = bytes;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    D3D11_SUBRESOURCE_DATA init{vtx.data(), 0, 0};
    m_dev.device()->CreateBuffer(&bd, &init, m_debugVB.ReleaseAndGetAddressOf());

    // Set transform for lines (world = identity)
    XMMATRIX I = XMMatrixIdentity();
    const Camera &cam = getActiveCamera();
    XMMATRIX V = cam.getViewMatrix();
    float aspect = (float) m_dev.width() / (float) m_dev.height();
    XMMATRIX P = cam.getProjectionMatrix(aspect);
    TransformCB cb{};
    XMStoreFloat4x4(&cb.World, I);
    XMStoreFloat4x4(&cb.WVP, I * V * P);
    m_dev.context()->UpdateSubresource(m_cbTransform.Get(), 0, nullptr, &cb, 0, 0);

    // Light CB unchanged
    LightCB cbLight{};
    cbLight.lightDir = m_lightDir;
    cbLight.lightColor = m_lightColor;
    cbLight.ambientColor = m_ambientColor;
    m_dev.context()->UpdateSubresource(m_cbLight.Get(), 0, nullptr, &cbLight, 0, 0);

    // Material CB for contact gizmo (white)
    MaterialCB cbMaterial{};
    cbMaterial.baseColorFactor = XMFLOAT4(1, 1, 1, 1);
    m_dev.context()->UpdateSubresource(m_cbMaterial.Get(), 0, nullptr, &cbMaterial, 0, 0);

    // Bind shader and CBs
    m_shader.bind(m_dev.context());
    ID3D11Buffer *cbs[] = {m_cbTransform.Get(), m_cbLight.Get(), m_cbMaterial.Get()};
    m_dev.context()->VSSetConstantBuffers(0, 3, cbs);
    m_dev.context()->PSSetConstantBuffers(0, 3, cbs);

    // Bind default texture and sampler
    ID3D11ShaderResourceView *srv = m_defaultTexture.srv();
    m_dev.context()->PSSetShaderResources(0, 1, &srv);
    ID3D11SamplerState *samp = m_sampler.Get();
    m_dev.context()->PSSetSamplers(0, 1, &samp);

    // Set RTs and viewport
    ID3D11RenderTargetView *rtv = m_dev.rtv();
    ID3D11DepthStencilView *dsv = m_dev.dsv();
    m_dev.context()->OMSetRenderTargets(1, &rtv, dsv);
    D3D11_VIEWPORT vp{0, 0, (FLOAT) m_dev.width(), (FLOAT) m_dev.height(), 0, 1};
    m_dev.context()->RSSetViewports(1, &vp);

    // IA setup and draw
    UINT stride = sizeof(DebugLineVertex), offset = 0;
    ID3D11Buffer *vb = m_debugVB.Get();
    m_dev.context()->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    m_dev.context()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    m_dev.context()->Draw((UINT) vtx.size(), 0);

    ID3D11ShaderResourceView *nullSRV[1] = {nullptr};
    m_dev.context()->PSSetShaderResources(0, 1, nullSRV);
}

void Renderer::drawContactsGizmo(const std::vector<OverlapResult> &contacts,
                                 const DirectX::XMFLOAT4 &color,
                                 float scale) {
    for (const auto &c: contacts) drawContactGizmo(c, color, scale);
}

void Renderer::drawMesh(const Mesh &mesh, const DirectX::XMMATRIX &transform, const Texture &texture,
                        const DirectX::XMFLOAT4 *baseColorFactor) {
    using namespace DirectX;

    // Get view matrix from camera and calculate projection
    const Camera &cam = getActiveCamera();
    XMMATRIX V = cam.getViewMatrix();
    float aspect = (float) m_dev.width() / (float) m_dev.height();
    XMMATRIX P = cam.getProjectionMatrix(aspect);

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

    // Update material constant buffer
    MaterialCB cbMaterial{};
    cbMaterial.baseColorFactor = baseColorFactor ? *baseColorFactor : XMFLOAT4(1, 1, 1, 1);
    m_dev.context()->UpdateSubresource(m_cbMaterial.Get(), 0, nullptr, &cbMaterial, 0, 0);

    // Bind shader
    m_shader.bind(m_dev.context());

    // Bind constant buffers
    ID3D11Buffer *cbs[] = {m_cbTransform.Get(), m_cbLight.Get(), m_cbMaterial.Get()};
    m_dev.context()->VSSetConstantBuffers(0, 3, cbs);
    m_dev.context()->PSSetConstantBuffers(0, 3, cbs);

    // Bind texture and sampler (prefer provided texture, fallback to default)
    ID3D11ShaderResourceView *srv = texture.isValid() ? texture.srv() : m_defaultTexture.srv();
    static int drawCount = 0;
    /*if (drawCount < 10) {
        // Print first 10 draws
        printf("Draw #%d - Texture valid: %d, SRV: %p, Default SRV: %p\n",
               drawCount++, texture.isValid(), texture.srv(), m_defaultTexture.srv());
    }*/
    m_dev.context()->PSSetShaderResources(0, 1, &srv);
    ID3D11SamplerState *samp = m_sampler.Get();
    m_dev.context()->PSSetSamplers(0, 1, &samp);

    // Set render targets and viewport
    ID3D11RenderTargetView *rtv = m_dev.rtv();
    ID3D11DepthStencilView *dsv = m_dev.dsv();
    m_dev.context()->OMSetRenderTargets(1, &rtv, dsv);

    D3D11_VIEWPORT vp{0, 0, (FLOAT) m_dev.width(), (FLOAT) m_dev.height(), 0, 1};
    m_dev.context()->RSSetViewports(1, &vp);

    // Draw mesh using its data
    UINT stride = mesh.stride();
    UINT offset = 0;
    ID3D11Buffer *vb = mesh.vertexBuffer();
    m_dev.context()->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    m_dev.context()->IASetIndexBuffer(mesh.indexBuffer(), DXGI_FORMAT_R16_UINT, 0);
    m_dev.context()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_dev.context()->DrawIndexed(mesh.indexCount(), 0, 0);

    ID3D11ShaderResourceView *nullSRV[1] = {nullptr};
    m_dev.context()->PSSetShaderResources(0, 1, nullSRV);
}

// ==== Render State Management for Transparent Objects ====

void Renderer::setAlphaBlending(bool enable) {
    if (enable && m_alphaBlendState) {
        float blendFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        m_dev.context()->OMSetBlendState(m_alphaBlendState.Get(), blendFactor, 0xFFFFFFFF);
    } else {
        // Disable blending
        m_dev.context()->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    }
}

void Renderer::setDepthWrite(bool enable) {
    if (enable) {
        // Enable depth write (normal opaque rendering)
        if (m_depthState) {
            m_dev.context()->OMSetDepthStencilState(m_depthState.Get(), 0);
        }
    } else {
        // Disable depth write but keep depth test (for transparent objects)
        if (m_depthNoWriteState) {
            m_dev.context()->OMSetDepthStencilState(m_depthNoWriteState.Get(), 0);
        }
    }
}

void Renderer::setBackfaceCulling(bool enable) {
    if (enable) {
        // Default back-face culling (created in initialize)
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> rs;
        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_BACK;
        rd.FrontCounterClockwise = FALSE;
        m_dev.device()->CreateRasterizerState(&rd, rs.GetAddressOf());
        m_dev.context()->RSSetState(rs.Get());
    } else {
        // No culling (double-sided rendering for billboards)
        if (m_noCullRasterizerState) {
            m_dev.context()->RSSetState(m_noCullRasterizerState.Get());
        }
    }
}

// ==== Ribbon/Trail Rendering ====

void Renderer::drawRibbon(const std::vector<RibbonVertex> &vertices,
                          const std::vector<uint16_t> &indices,
                          ID3D11ShaderResourceView *texture,
                          const DirectX::XMFLOAT4 &baseColor) {
    if (vertices.empty() || indices.empty()) return;

    UINT vbBytes = static_cast<UINT>(vertices.size() * sizeof(RibbonVertex));
    UINT ibBytes = static_cast<UINT>(indices.size() * sizeof(uint16_t));

    // Recreate or resize vertex buffer if needed
    if (!m_ribbonVB || m_ribbonVBCapacity < vbBytes) {
        m_ribbonVB.Reset();
        m_ribbonVBCapacity = vbBytes * 2; // Reserve extra space to reduce reallocations
        D3D11_BUFFER_DESC vbDesc{};
        vbDesc.ByteWidth = m_ribbonVBCapacity;
        vbDesc.Usage = D3D11_USAGE_DYNAMIC;
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(m_dev.device()->CreateBuffer(&vbDesc, nullptr, m_ribbonVB.ReleaseAndGetAddressOf()))) {
            return;
        }
    }

    // Recreate or resize index buffer if needed
    if (!m_ribbonIB || m_ribbonIBCapacity < ibBytes) {
        m_ribbonIB.Reset();
        m_ribbonIBCapacity = ibBytes * 2; // Reserve extra space
        D3D11_BUFFER_DESC ibDesc{};
        ibDesc.ByteWidth = m_ribbonIBCapacity;
        ibDesc.Usage = D3D11_USAGE_DYNAMIC;
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        ibDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(m_dev.device()->CreateBuffer(&ibDesc, nullptr, m_ribbonIB.ReleaseAndGetAddressOf()))) {
            return;
        }
    }

    // Upload vertex data
    D3D11_MAPPED_SUBRESOURCE mappedVB{};
    if (SUCCEEDED(m_dev.context()->Map(m_ribbonVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedVB))) {
        memcpy(mappedVB.pData, vertices.data(), vbBytes);
        m_dev.context()->Unmap(m_ribbonVB.Get(), 0);
    } else {
        return;
    }

    // Upload index data
    D3D11_MAPPED_SUBRESOURCE mappedIB{};
    if (SUCCEEDED(m_dev.context()->Map(m_ribbonIB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedIB))) {
        memcpy(mappedIB.pData, indices.data(), ibBytes);
        m_dev.context()->Unmap(m_ribbonIB.Get(), 0);
    } else {
        return;
    }

    // Set transform (identity world, ribbon vertices are already in world space)
    XMMATRIX I = XMMatrixIdentity();
    const Camera &cam = getActiveCamera();
    XMMATRIX V = cam.getViewMatrix();
    float aspect = static_cast<float>(m_dev.width()) / static_cast<float>(m_dev.height());
    XMMATRIX P = cam.getProjectionMatrix(aspect);

    TransformCB cbTransform{};
    XMStoreFloat4x4(&cbTransform.World, I);
    XMStoreFloat4x4(&cbTransform.WVP, I * V * P);
    m_dev.context()->UpdateSubresource(m_cbTransform.Get(), 0, nullptr, &cbTransform, 0, 0);

    // Set light
    LightCB cbLight{};
    cbLight.lightDir = m_lightDir;
    cbLight.lightColor = m_lightColor;
    cbLight.ambientColor = m_ambientColor;
    m_dev.context()->UpdateSubresource(m_cbLight.Get(), 0, nullptr, &cbLight, 0, 0);

    // Set material
    MaterialCB cbMaterial{};
    cbMaterial.baseColorFactor = baseColor;
    m_dev.context()->UpdateSubresource(m_cbMaterial.Get(), 0, nullptr, &cbMaterial, 0, 0);

    // Bind shader
    m_shader.bind(m_dev.context());

    // Bind constant buffers
    ID3D11Buffer *cbs[] = {m_cbTransform.Get(), m_cbLight.Get(), m_cbMaterial.Get()};
    m_dev.context()->VSSetConstantBuffers(0, 3, cbs);
    m_dev.context()->PSSetConstantBuffers(0, 3, cbs);

    // Bind texture (use provided texture or default white)
    ID3D11ShaderResourceView *srv = texture ? texture : m_defaultTexture.srv();
    m_dev.context()->PSSetShaderResources(0, 1, &srv);
    ID3D11SamplerState *samp = m_sampler.Get();
    m_dev.context()->PSSetSamplers(0, 1, &samp);

    // Set render targets and viewport
    ID3D11RenderTargetView *rtv = m_dev.rtv();
    ID3D11DepthStencilView *dsv = m_dev.dsv();
    m_dev.context()->OMSetRenderTargets(1, &rtv, dsv);
    D3D11_VIEWPORT vp{0, 0, static_cast<FLOAT>(m_dev.width()), static_cast<FLOAT>(m_dev.height()), 0, 1};
    m_dev.context()->RSSetViewports(1, &vp);

    // Bind vertex/index buffers
    UINT stride = sizeof(RibbonVertex);
    UINT offset = 0;
    m_dev.context()->IASetVertexBuffers(0, 1, m_ribbonVB.GetAddressOf(), &stride, &offset);
    m_dev.context()->IASetIndexBuffer(m_ribbonIB.Get(), DXGI_FORMAT_R16_UINT, 0);
    m_dev.context()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Draw
    m_dev.context()->DrawIndexed(static_cast<UINT>(indices.size()), 0, 0);

    // Cleanup
    ID3D11ShaderResourceView *nullSRV[1] = {nullptr};
    m_dev.context()->PSSetShaderResources(0, 1, nullSRV);
}
