#pragma once
#include <DirectXMath.h>
#include <d3d11.h>
#include <cstdint>

// Minimal material placeholder
struct Material {
    ID3D11ShaderResourceView *baseColor = nullptr;
    DirectX::XMFLOAT4 baseColorFactor{1, 1, 1, 1};
    bool transparent = false; // reserved
    uint32_t techniqueKey = 0; // reserved
};
