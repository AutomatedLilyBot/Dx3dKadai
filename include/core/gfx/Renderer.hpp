#pragma once
#include "RenderDeviceD3D11.hpp"
#include "ShaderProgram.hpp"
#include "Mesh.hpp"
#include <DirectXMath.h>

#include "ShaderProgram.hpp"


class Renderer {
public:
    struct SpriteDesc3D { // 兼容命名：用它替代原 drawSprite 的参数
        DirectX::XMFLOAT3 position{0,0,0};
        DirectX::XMFLOAT2 size{2,2};
        float rotationY{0}; // 简单示例：绕 Y 轴旋转
        DirectX::XMFLOAT3 albedo{1,1,1};
    };

    bool initialize(HWND hwnd, unsigned width, unsigned height, bool debug=false);
    void shutdown();


    void beginFrame(float r=0.2f, float g=0.3f, float b=0.4f, float a=1.0f);
    void endFrame();


    // 旧 drawSprite 的替代：画一个 3D 带光照矩形
    void drawSprite(const SpriteDesc3D& d);


private:
    // CPU 侧常量缓冲结构（与 HLSL 对齐）
    struct alignas(16) FrameCB {
        DirectX::XMFLOAT4X4 view;
        DirectX::XMFLOAT4X4 proj;
        DirectX::XMFLOAT3 camPos; float _pad0{};
        DirectX::XMFLOAT3 ambient; float _pad1{};
        DirectX::XMFLOAT3 dirDir; float dirIntensity{};
        DirectX::XMFLOAT3 dirColor; float _pad2{};
        UINT pointCount{0}; DirectX::XMFLOAT3 _pad3{};
    };


    struct alignas(16) ObjectCB {
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4X4 worldNormal;
    };


    struct alignas(16) MaterialCB {
        DirectX::XMFLOAT3 albedoFactor{1,1,1}; float useTex{0};
        DirectX::XMFLOAT3 specColor{1,1,1}; float specPower{64.f};
        DirectX::XMFLOAT3 emissive{0,0,0}; float _pad{};
    };


    struct alignas(16) PointLightCPU {
        DirectX::XMFLOAT3 positionWS; float intensity{1};
        DirectX::XMFLOAT3 color{1,1,1}; float range{5};
    };


    struct alignas(16) PointLightsCB {
        PointLightCPU lights[4];
    };


    RenderDeviceD3D11 m_dev;
    ShaderProgram m_phong;
    Mesh m_quad;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbFrame;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbObject;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbMaterial;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbPoint;


    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampler; // for optional albedo texture


    // Cached state
    DirectX::XMFLOAT3 m_cameraPos{0,0,-3};
};
