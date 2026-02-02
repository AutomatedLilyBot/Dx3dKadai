#include "Camera.hpp"

#include <cstdlib>

using namespace DirectX;

Camera::Camera() {
    updateVectors();
}

void Camera::processKeyboard(bool forward, bool backward, bool left, bool right, bool rotateLeft, bool rotateRight, bool boost, float deltaTime) {
    // 正在切换摄像机时不响应键盘输入
    if (m_isSwitchingCamera) {
        return;
    }

    // 固定摄像机不响应键盘输入
    if (m_cameraType == CameraType::FrontView || m_cameraType == CameraType::TopView) {
        return;
    }

    if (m_mode == CameraMode::RTS) {
        // RTS 模式：WASD 在水平面上平移，QE 旋转
        float effectiveSpeed = boost ? (m_moveSpeed * m_moveSpeedBoost) : m_moveSpeed;
        float velocity = effectiveSpeed * deltaTime;

        // 计算水平前方和右方向（忽略 Y 分量）
        XMFLOAT3 horizontalForward{sinf(m_yaw), 0, cosf(m_yaw)};
        XMFLOAT3 horizontalRight{cosf(m_yaw), 0, -sinf(m_yaw)};

        XMVECTOR pos = XMLoadFloat3(&m_position);
        XMVECTOR fwd = XMLoadFloat3(&horizontalForward);
        XMVECTOR rgt = XMLoadFloat3(&horizontalRight);

        if (forward)  pos = XMVectorAdd(pos, XMVectorScale(fwd, velocity));
        if (backward) pos = XMVectorSubtract(pos, XMVectorScale(fwd, velocity));
        if (right)    pos = XMVectorAdd(pos, XMVectorScale(rgt, velocity));
        if (left)     pos = XMVectorSubtract(pos, XMVectorScale(rgt, velocity));

        // QE 旋转
        if (rotateLeft)  m_yaw -= m_rotateSpeed * deltaTime;
        if (rotateRight) m_yaw += m_rotateSpeed * deltaTime;

        XMStoreFloat3(&m_position, pos);

        // 保持固定高度和俯视角（仅在自由摄像机模式）
        if (m_cameraType == CameraType::FreeCam) {
            m_position.y = m_rtsHeight;
            m_pitch = m_rtsPitch;
        }

        m_targetPosition = m_position;
        updateVectors();
    }
    else if (m_mode == CameraMode::Orbit) {
        // Orbit 模式：禁用键盘移动
        return;
    }
}

// RTS 模式不需要鼠标视角控制，已移除 processMouseMove

void Camera::processMouseScroll(float delta) {
    // 在 FreeCam 模式下，滚轮调整摄像机高度
    if (m_mode == CameraMode::RTS && m_cameraType == CameraType::FreeCam) {
        m_rtsHeight -= delta * m_heightAdjustSpeed;

        // 限制高度范围
        if (m_rtsHeight < m_freeCamMinHeight) m_rtsHeight = m_freeCamMinHeight;
        if (m_rtsHeight > m_freeCamMaxHeight) m_rtsHeight = m_freeCamMaxHeight;

        // 立即应用高度变化
        m_position.y = m_rtsHeight;
        m_targetPosition.y = m_rtsHeight;
    }
    else {
        // 其他模式下，滚轮调整移动速度
        m_moveSpeed += delta * 0.5f;
        if (m_moveSpeed < 0.5f) m_moveSpeed = 0.5f;
        if (m_moveSpeed > 50.0f) m_moveSpeed = 50.0f;
    }
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
    // 应用抖动偏移
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR shakeOffset = XMLoadFloat3(&m_shakeOffset);
    pos = XMVectorAdd(pos, shakeOffset);

    XMVECTOR fwd = XMLoadFloat3(&m_forward);
    XMVECTOR up = XMLoadFloat3(&m_up);

    XMVECTOR target = XMVectorAdd(pos, fwd);

    return XMMatrixLookAtLH(pos, target, up);
}

XMMATRIX Camera::getProjectionMatrix(float aspectRatio) const {
    // 使用透视投影矩阵
    // fov: 视野角度（弧度）
    // aspectRatio: 宽高比（width/height）
    // nearPlane: 近裁剪面距离
    // farPlane: 远裁剪面距离
    return XMMatrixPerspectiveFovLH(m_fov, aspectRatio, m_nearPlane, m_farPlane);
}

void Camera::setLookAt(const DirectX::XMFLOAT3 &target) {
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR tgt = XMLoadFloat3(&target);
    XMVECTOR dir = XMVector3Normalize(XMVectorSubtract(tgt, pos));

    XMFLOAT3 direction;
    XMStoreFloat3(&direction, dir);

    // Calculate yaw and pitch from direction
    m_yaw = atan2f(direction.x, direction.z);
    m_pitch = asinf(direction.y);

    updateVectors();
}

void Camera::setOrbitTarget(const DirectX::XMFLOAT3& target) {
    m_orbitTarget = target;

    // 计算目标位置：在 Node 后方俯视
    // 后方 = target + (0, 3, -5) in world space
    m_targetPosition = {
        target.x,
        target.y + 3.0f,
        target.z - 5.0f
    };

    // 启动过渡动画
    m_isTransitioning = true;
}

void Camera::update(float dt) {
    // 摄像机切换动画
    if (m_isSwitchingCamera) {
        float lerpSpeed = dt * m_transitionSpeed;

        // 插值位置
        XMVECTOR current = XMLoadFloat3(&m_position);
        XMVECTOR target = XMLoadFloat3(&m_targetPosition);
        XMVECTOR newPos = XMVectorLerp(current, target, lerpSpeed);
        XMStoreFloat3(&m_position, newPos);

        // 插值欧拉角
        m_yaw = m_yaw + (m_targetYaw - m_yaw) * lerpSpeed;
        m_pitch = m_pitch + (m_targetPitch - m_pitch) * lerpSpeed;

        // 检查是否足够接近目标
        XMVECTOR diff = XMVectorSubtract(target, newPos);
        float distance = XMVectorGetX(XMVector3Length(diff));
        float angleDiff = fabsf(m_yaw - m_targetYaw) + fabsf(m_pitch - m_targetPitch);

        if (distance < 0.01f && angleDiff < 0.01f) {
            // 切换完成
            m_position = m_targetPosition;
            m_yaw = m_targetYaw;
            m_pitch = m_targetPitch;
            m_cameraType = m_targetCameraType;
            m_isSwitchingCamera = false;
        }

        updateVectors();
        return; // 切换过程中不执行其他逻辑
    }
    // 仅在过渡动画期间进行平滑插值（Focus功能）
    else if (m_isTransitioning) {
        XMVECTOR current = XMLoadFloat3(&m_position);
        XMVECTOR target = XMLoadFloat3(&m_targetPosition);

        // 平滑插值
        XMVECTOR newPos = XMVectorLerp(current, target, dt * m_transitionSpeed);
        XMStoreFloat3(&m_position, newPos);

        // 检查是否足够接近目标（停止过渡）
        XMVECTOR diff = XMVectorSubtract(target, newPos);
        float distance = XMVectorGetX(XMVector3Length(diff));
        if (distance < 0.01f) {
            m_position = m_targetPosition;
            m_isTransitioning = false;
        }
    }

    // Orbit 模式下始终朝向目标（仅在非切换状态）
    if (m_mode == CameraMode::Orbit && !m_isSwitchingCamera) {
        setLookAt(m_orbitTarget);
    }

    // 处理摄像机抖动
    if (m_isShaking) {
        m_shakeTimer += dt;

        if (m_shakeTimer >= m_shakeDuration) {
            // 抖动结束
            m_isShaking = false;
            m_shakeOffset = XMFLOAT3{0, 0, 0};
        } else {
            // 计算衰减系数（随时间减弱）
            float progress = m_shakeTimer / m_shakeDuration;
            float decay = 1.0f - progress; // 线性衰减

            // 生成随机偏移量（使用简单的伪随机）
            float randomX = (static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f) * m_shakeIntensity * decay;
            float randomY = (static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f) * m_shakeIntensity * decay;
            float randomZ = (static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f) * m_shakeIntensity * decay;

            m_shakeOffset = XMFLOAT3{randomX, randomY, randomZ};
        }
    }
}

void Camera::focusOnTarget(const DirectX::XMFLOAT3& target) {
    // 计算相机到目标的水平方向向量（保持当前高度、俯仰角和 yaw）
    // 目标位置 = target - forward * distance（距离基于当前 forward 方向）

    // 计算水平前方向（使用当前 yaw，忽略 pitch）
    XMFLOAT3 horizontalForward{sinf(m_yaw), 0, cosf(m_yaw)};

    // 设定观察距离（可以根据需要调整）
    float viewDistance = 8.0f;

    // 计算相机的新目标位置：从 target 往后退 viewDistance
    XMVECTOR targetVec = XMLoadFloat3(&target);
    XMVECTOR forwardVec = XMLoadFloat3(&horizontalForward);
    XMVECTOR newPosVec = XMVectorSubtract(targetVec, XMVectorScale(forwardVec, viewDistance));

    // 保持当前高度（Y 坐标不变）
    XMFLOAT3 newPos;
    XMStoreFloat3(&newPos, newPosVec);
    newPos.y = m_position.y;  // 保持当前高度

    // 设置目标位置并启动平滑过渡
    m_targetPosition = newPos;
    m_isTransitioning = true;

    // pitch 和 yaw 保持不变（在 update() 中不会修改）
}

void Camera::switchToCamera(CameraType type) {
    if (type == m_cameraType && !m_isSwitchingCamera) {
        return; // 已经是目标摄像机，无需切换
    }

    m_targetCameraType = type;
    m_isSwitchingCamera = true;

    // 根据摄像机类型设置目标位置和朝向
    switch (type) {
        case CameraType::FreeCam:
            // 自由摄像机：返回RTS默认位置
            m_targetPosition = XMFLOAT3{0, 10.0f, -10.0f};
            m_targetYaw = 0.0f;
            m_targetPitch = -DirectX::XM_PIDIV4; // -45°
            break;

        case CameraType::FrontView:
            // 前方俯视摄像机
            m_targetPosition = XMFLOAT3{0, 12.0f, -20.0f};
            m_targetYaw = 0.0f; // 朝向+Z
            m_targetPitch = -DirectX::XM_PIDIV2 / 3.0f; // -30°
            break;

        case CameraType::TopView:
            // 中央下视摄像机
            m_targetPosition = XMFLOAT3{0, 25.0f, 0};
            m_targetYaw = 0.0f;
            m_targetPitch = -DirectX::XM_PIDIV2 * 0.95f; // -85°（接近正下方）
            break;
    }
}

void Camera::triggerShake(float intensity, float duration) {
    m_isShaking = true;
    m_shakeIntensity = intensity;
    m_shakeDuration = duration;
    m_shakeTimer = 0.0f;
}

