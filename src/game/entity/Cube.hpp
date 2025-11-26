//
// Created by zyzyz on 2025/10/30.
//

#ifndef GOLFGAME_CUBE_HPP
#define GOLFGAME_CUBE_HPP
#include "../../core/gfx/Mesh.hpp"
#include "../../core/gfx/Texture.hpp"
#include <DirectXMath.h>


class Cube {
public:
    Cube() = default;

    ~Cube() = default;

    // Initialize the cube with a device
    bool initialize(ID3D11Device *device);

    // Getters
    Mesh &getMesh() { return m_mesh; }
    const Mesh &getMesh() const { return m_mesh; }

    Texture &getTexture() { return m_texture; }
    const Texture &getTexture() const { return m_texture; }

    DirectX::XMMATRIX getTransform() const;

    // Transform setters
    void setPosition(float x, float y, float z);

    void setRotation(float pitch, float yaw, float roll);

    void setScale(float x, float y, float z);

    void setScale(float uniformScale);

private:
    Mesh m_mesh;
    Texture m_texture;

    // Transform components
    DirectX::XMFLOAT3 m_position = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_rotation = {0.0f, 0.0f, 0.0f}; // pitch, yaw, roll
    DirectX::XMFLOAT3 m_scale = {1.0f, 1.0f, 1.0f};
};


#endif //GOLFGAME_CUBE_HPP