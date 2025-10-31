#pragma once
#include "RenderDeviceD3D11.hpp"
#include "ShaderProgram.hpp"
#include "Mesh.hpp"
#include "Texture.hpp"
#include "Camera.hpp"
#include <DirectXMath.h>

class Renderer {
public:
    bool initialize(HWND hwnd, unsigned width, unsigned height, bool debug=false);
    void shutdown();
    void beginFrame(float r=0.1f,float g=0.1f,float b=0.15f,float a=1.0f);
    void endFrame();

    // Draw mesh with transform matrix and texture
    void drawMesh(const Mesh& mesh, const DirectX::XMMATRIX& transform, const Texture& texture);

    // Camera access
    Camera& getCamera() { return m_camera; }
    const Camera& getCamera() const { return m_camera; }

    // Temporary accessor for cube mesh (for demo purposes)
    const Mesh& getCubeMesh() const { return m_cube; }

    // Device accessor for resource initialization
    ID3D11Device* device() const { return m_dev.device(); }

private:
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
    ShaderProgram     m_shader;
    Mesh              m_cube;
    Texture           m_defaultTexture;
    Camera            m_camera;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbTransform;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbLight;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampler;

    DirectX::XMFLOAT3 m_lightDir{0.577f, -0.577f, 0.577f}; // Normalized diagonal
    DirectX::XMFLOAT3 m_lightColor{1.0f, 1.0f, 1.0f};
    DirectX::XMFLOAT3 m_ambientColor{0.2f, 0.2f, 0.2f};
};
