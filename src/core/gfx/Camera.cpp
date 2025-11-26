#include "Camera.hpp"

using namespace DirectX;

Camera::Camera() {
    updateVectors();
}

void Camera::processKeyboard(bool forward, bool backward, bool left, bool right, bool up, bool down, float deltaTime) {
    float velocity = m_moveSpeed * deltaTime;

    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR fwd = XMLoadFloat3(&m_forward);
    XMVECTOR rgt = XMLoadFloat3(&m_right);
    XMVECTOR upVec = XMLoadFloat3(&m_up);

    if (forward) pos = XMVectorAdd(pos, XMVectorScale(fwd, velocity));
    if (backward) pos = XMVectorSubtract(pos, XMVectorScale(fwd, velocity));
    if (right) pos = XMVectorAdd(pos, XMVectorScale(rgt, velocity));
    if (left) pos = XMVectorSubtract(pos, XMVectorScale(rgt, velocity));
    if (up) pos = XMVectorAdd(pos, XMVectorScale(upVec, velocity));
    if (down) pos = XMVectorSubtract(pos, XMVectorScale(upVec, velocity));

    XMStoreFloat3(&m_position, pos);
}

void Camera::processMouseMove(float deltaX, float deltaY, float sensitivity) {
    m_yaw += deltaX * sensitivity;
    m_pitch += deltaY * sensitivity;

    // Constrain pitch to avoid gimbal lock
    const float maxPitch = XM_PIDIV2 - 0.01f;
    if (m_pitch > maxPitch) m_pitch = maxPitch;
    if (m_pitch < -maxPitch) m_pitch = -maxPitch;

    updateVectors();
}

void Camera::processMouseScroll(float delta) {
    m_moveSpeed += delta * 0.5f;
    if (m_moveSpeed < 0.5f) m_moveSpeed = 0.5f;
    if (m_moveSpeed > 50.0f) m_moveSpeed = 50.0f;
}

void Camera::updateVectors() {
    // Calculate forward vector from yaw and pitch
    // Yaw=0 looks at +Z, Yaw=90° looks at +X
    XMFLOAT3 forward;
    forward.x = cosf(m_pitch) * sinf(m_yaw);
    forward.y = sinf(m_pitch);
    forward.z = cosf(m_pitch) * cosf(m_yaw);

    XMVECTOR fwd = XMVector3Normalize(XMLoadFloat3(&forward));
    XMStoreFloat3(&m_forward, fwd);

    // Calculate right vector (cross forward with world up)
    XMVECTOR worldUp = XMLoadFloat3(&m_worldUp);
    XMVECTOR rgt = XMVector3Normalize(XMVector3Cross(worldUp, fwd));
    XMStoreFloat3(&m_right, rgt);

    // Calculate up vector (cross right with forward)
    XMVECTOR up = XMVector3Normalize(XMVector3Cross(fwd, rgt));
    XMStoreFloat3(&m_up, up);
}

XMMATRIX Camera::getViewMatrix() const {
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR fwd = XMLoadFloat3(&m_forward);
    XMVECTOR up = XMLoadFloat3(&m_up);

    XMVECTOR target = XMVectorAdd(pos, fwd);

    return XMMatrixLookAtLH(pos, target, up);
}

void Camera::setLookAt(const DirectX::XMFLOAT3 &target) {
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR tgt = XMLoadFloat3(&target);
    XMVECTOR dir = XMVector3Normalize(XMVectorSubtract(tgt, pos));

    XMFLOAT3 direction;
    XMStoreFloat3(&direction, dir);

    // Calculate yaw and pitch from direction
    m_yaw = atan2f(direction.z, direction.x);
    m_pitch = asinf(direction.y);

    updateVectors();
}