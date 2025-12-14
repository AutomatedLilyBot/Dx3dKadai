#pragma once
#include "RenderDeviceD3D11.hpp"
#include "ShaderProgram.hpp"
#include "Mesh.hpp"
#include "Texture.hpp"
#include "Camera.hpp"
#include "Model.hpp"
#include "../physics/Collider.hpp"
#include <DirectXMath.h>

// Forward declare to avoid heavy include in header
struct IDrawable;

class Renderer {
public:
    bool initialize(HWND hwnd, unsigned width, unsigned height, bool debug = false);

    void shutdown();

    void beginFrame(float r = 0.1f, float g = 0.1f, float b = 0.15f, float a = 1.0f);

    void endFrame();

    // Draw mesh with transform matrix and texture
    void drawMesh(const Mesh &mesh, const DirectX::XMMATRIX &transform, const Texture &texture);

    // Draw a full model: iterate draw items, combine model transform with node transforms
    void drawModel(const Model &model, const DirectX::XMMATRIX &modelTransform);

    // Draw via drawable interface (adapter)
    void draw(const IDrawable &drawable);

    // Debug draw helpers for colliders (wireframe via line list)
    void drawColliderWire(const ColliderBase &col);

    void drawCollidersWire(const std::vector<ColliderBase *> &cols);

    // Draw collision contact info (generated per-frame). No per-contact toggle; just draw when called.
    // It visualizes:
    // - pointOnA/pointOnB as small crosses
    // - a line between pointOnA and pointOnB
    // - contact normal (from midpoint towards normal) with an arrow; length ~ max(penetration, minLen)
    void drawContactGizmo(const OverlapResult &contact,
                          const DirectX::XMFLOAT4 &color = DirectX::XMFLOAT4{1, 1, 0, 1},
                          float scale = 1.0f);

    // Batch version
    void drawContactsGizmo(const std::vector<OverlapResult> &contacts,
                           const DirectX::XMFLOAT4 &color = DirectX::XMFLOAT4{1, 1, 0, 1},
                           float scale = 1.0f);

    // Camera management: 现在需要外部提供 Camera
    // 旧接口保留用于兼容性，但应该使用 setCamera()
    [[deprecated("Use setCamera() and pass Camera from Scene")]]
    Camera &getCamera() { return m_camera; }
    [[deprecated("Use setCamera() and pass Camera from Scene")]]
    const Camera &getCamera() const { return m_camera; }

    // 设置当前帧使用的 Camera（由 Scene 提供）
    void setCamera(const Camera* camera) { m_externalCamera = camera; }

    // Temporary accessor for cube mesh (for demo purposes)
    const Mesh &getCubeMesh() const { return m_cube; }

    // Convenience: draw the internal unit cube mesh with the default texture
    // Transform is provided by caller (world matrix)
    void drawCube(const DirectX::XMMATRIX &worldTransform);

    // Device accessor for resource initialization
    ID3D11Device *device() const { return m_dev.device(); }

private:
    // 获取当前活跃的 Camera（优先外部，回退到内部）
    const Camera& getActiveCamera() const {
        return m_externalCamera ? *m_externalCamera : m_camera;
    }

    struct TransformCB {
        DirectX::XMFLOAT4X4 World;
        DirectX::XMFLOAT4X4 WVP;
    };

    struct LightCB {
        DirectX::XMFLOAT3 lightDir;
        float padding1;
        DirectX::XMFLOAT3 lightColor;
        float padding2;
        DirectX::XMFLOAT3 ambientColor;
        float padding3;
    };

    RenderDeviceD3D11 m_dev;
    ShaderProgram m_shader;
    Mesh m_cube;
    Texture m_defaultTexture;
    Camera m_camera; // 保留作为后备，用于兼容旧代码
    const Camera* m_externalCamera = nullptr; // 外部 Camera（优先使用）

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbTransform;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbLight;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampler;

    // Dynamic vertex buffer for debug lines
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_debugVB;

    DirectX::XMFLOAT3 m_lightDir{0.577f, -0.577f, 0.577f}; // Normalized diagonal
    DirectX::XMFLOAT3 m_lightColor{1.0f, 1.0f, 1.0f};
    DirectX::XMFLOAT3 m_ambientColor{0.2f, 0.2f, 0.2f};
};