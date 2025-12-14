//
// Created by zyzyz on 2025/10/30.
//

#include "Cube.hpp"
#include <string>
#include <windows.h>

using namespace DirectX;

static std::wstring ExeDirCube() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring s(buf);
    auto p = s.find_last_of(L"\\/");
    return (p == std::wstring::npos) ? L"." : s.substr(0, p);
}

bool Cube::initialize(ID3D11Device* device)
{
    // Create a fixed cube mesh
    if (!m_mesh.createCube(device, 1.0f)) {
        return false;
    }

    // Try to load a fixed texture from asset directory; fallback to white if not found
    std::wstring texPath = ExeDirCube() + L"\\asset\\block_field.png";
    //wprintf(L"Attempting to load texture from: %s\n", texPath.c_str());
    if (!m_texture.loadFromFile(device, texPath)) {
        wprintf(L"Failed to load texture, using white fallback\n");
        // Fallback to white texture if loading fails
        if (!m_texture.createSolidColor(device, 255, 255, 255, 255)) {
            return false;
        }
    } else {
        //wprintf(L"Texture loaded successfully!\n");
    }

    return true;
}

// Transform setters
void Cube::setPosition(float x, float y, float z) { m_position = XMFLOAT3(x, y, z); }
void Cube::setRotation(float pitch, float yaw, float roll) { m_rotation = XMFLOAT3(pitch, yaw, roll); }
void Cube::setScale(float x, float y, float z) { m_scale = XMFLOAT3(x, y, z); }
void Cube::setScale(float uniformScale) { m_scale = XMFLOAT3(uniformScale, uniformScale, uniformScale); }

DirectX::XMMATRIX Cube::getTransform() const
{
    XMMATRIX S = XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(m_rotation.x, m_rotation.y, m_rotation.z);
    XMMATRIX T = XMMatrixTranslation(m_position.x, m_position.y, m_position.z);
    return S * R * T;
}
