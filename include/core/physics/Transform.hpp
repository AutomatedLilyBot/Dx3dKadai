#pragma once
#include <DirectXMath.h>

// Standalone Transform container
struct Transform {
    DirectX::XMFLOAT3 position{0,0,0};
    DirectX::XMFLOAT3 rotationEuler{0,0,0}; // pitch (x), yaw (y), roll (z)
    DirectX::XMFLOAT3 scale{1,1,1};

    DirectX::XMMATRIX world() const {
        using namespace DirectX;
        XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
        XMMATRIX R = XMMatrixRotationRollPitchYaw(rotationEuler.x, rotationEuler.y, rotationEuler.z);
        XMMATRIX T = XMMatrixTranslation(position.x, position.y, position.z);
        return S * R * T;
    }
};
