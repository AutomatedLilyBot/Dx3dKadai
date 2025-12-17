//
// Created by zyzyz on 2025/10/26.
//

#pragma once
#include <DirectXMath.h>

struct VertexPC {
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT4 col;
};

struct VertexPNV {
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT3 norm;
    DirectX::XMFLOAT2 uv;
};

struct VertexPNCT {
    DirectX::XMFLOAT3 pos; // Position
    DirectX::XMFLOAT3 normal; // Normal
    DirectX::XMFLOAT4 col; // Color
    DirectX::XMFLOAT2 uv; // UV
};
