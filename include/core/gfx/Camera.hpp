#pragma once
#include <DirectXMath.h>

class Camera {
public:
    Camera();

    // Update camera based on input
    void processKeyboard(bool forward, bool backward, bool left, bool right, bool up, bool down, float deltaTime);
    void processMouseMove(float deltaX, float deltaY, float sensitivity = 0.002f);
    void processMouseScroll(float delta);

    // Get matrices
    DirectX::XMMATRIX getViewMatrix() const;
    DirectX::XMFLOAT3 getPosition() const { return m_position; }
    DirectX::XMFLOAT3 getForward() const { return m_forward; }
    DirectX::XMFLOAT3 getRight() const { return m_right; }
    DirectX::XMFLOAT3 getUp() const { return m_up; }

    // Set parameters
    void setPosition(const DirectX::XMFLOAT3& pos) { m_position = pos; updateVectors(); }
    void setMoveSpeed(float speed) { m_moveSpeed = speed; }
    void setLookAt(const DirectX::XMFLOAT3& target);

private:
    void updateVectors();

    // Camera position and orientation
    DirectX::XMFLOAT3 m_position{0.0f, 0.0f, -10.0f};
    DirectX::XMFLOAT3 m_forward{0.0f, 0.0f, 1.0f};
    DirectX::XMFLOAT3 m_right{1.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_up{0.0f, 1.0f, 0.0f};
    DirectX::XMFLOAT3 m_worldUp{0.0f, 1.0f, 0.0f};

    // Euler angles (in radians)
    float m_yaw = 0.0f;                   // Around Y axis (left/right), 0=look at +Z
    float m_pitch = 0.0f;                 // Around X axis (up/down)

    // Camera parameters
    float m_moveSpeed = 5.0f;
    float m_fov = DirectX::XM_PIDIV2 * 1.2f;  // 60 degrees
    float m_nearPlane = 0.1f;
    float m_farPlane = 100.0f;
};
