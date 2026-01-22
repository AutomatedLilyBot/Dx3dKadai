#pragma once
#include <DirectXMath.h>

// 相机模式
enum class CameraMode {
    RTS,    // RTS 风格：固定高度和俯视角，WASD 平移，QE 旋转
    Orbit   // 绕目标点旋转（选中 Node 后可选进入）
};

// 摄像机类型（用于多摄像机切换）
enum class CameraType {
    FreeCam, // 自由摄像机（可以WASD移动）
    FrontView, // 前方俯视摄像机（固定）
    TopView // 中央下视摄像机（固定）
};

class Camera {
public:
    Camera();

    // RTS-style input
    void processKeyboard(bool forward, bool backward, bool left, bool right, bool rotateLeft, bool rotateRight, bool boost, float deltaTime);
    void processMouseScroll(float delta);

    // Get matrices
    DirectX::XMMATRIX getViewMatrix() const;
    DirectX::XMMATRIX getProjectionMatrix(float aspectRatio) const;
    DirectX::XMFLOAT3 getPosition() const { return m_position; }
    DirectX::XMFLOAT3 getForward() const { return m_forward; }
    DirectX::XMFLOAT3 getRight() const { return m_right; }
    DirectX::XMFLOAT3 getUp() const { return m_up; }

    // Set parameters
    void setPosition(const DirectX::XMFLOAT3& pos) { m_position = pos; updateVectors(); }
    void setMoveSpeed(float speed) { m_moveSpeed = speed; }
    void setLookAt(const DirectX::XMFLOAT3& target);

    // Mode control
    void setMode(CameraMode mode) { m_mode = mode; }
    CameraMode getMode() const { return m_mode; }

    // Orbit mode control
    void setOrbitTarget(const DirectX::XMFLOAT3& target);
    void setOrbitDistance(float distance) { m_orbitDistance = distance; }
    void setOrbitPitchLimits(float minPitch, float maxPitch) {
        m_orbitPitchMin = minPitch;
        m_orbitPitchMax = maxPitch;
    }

    // Smooth camera transition
    void update(float dt); // 处理相机平滑过渡动画

    // Focus on target (F key): smoothly move camera to center the target
    void focusOnTarget(const DirectX::XMFLOAT3& target);

    // Camera switching
    void switchToCamera(CameraType type); // 切换到指定摄像机类型
    CameraType getCameraType() const { return m_cameraType; }

    // Camera shake
    void triggerShake(float intensity = 0.3f, float duration = 0.3f); // 触发画面抖动

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
    float m_moveSpeedBoost = 2.5f;  // 按住 Shift 时的速度倍数
    float m_fov = DirectX::XM_PIDIV2 * 1.2f;  // 60 degrees
    float m_nearPlane = 0.1f;
    float m_farPlane = 100.0f;

    // Camera mode
    CameraMode m_mode = CameraMode::RTS;

    // RTS mode parameters
    float m_rtsHeight = 8.0f;              // 固定高度
    float m_rtsPitch = -DirectX::XM_PIDIV4; // 固定俯视角度（-45°）
    float m_rotateSpeed = 1.0f;              // QE 旋转速度

    // Orbit mode parameters
    DirectX::XMFLOAT3 m_orbitTarget{0, 0, 0};
    float m_orbitDistance = 5.0f;
    float m_orbitPitchMin = -DirectX::XM_PIDIV2/3.0f;  // -30°
    float m_orbitPitchMax = DirectX::XM_PIDIV2/3.0f*2.0f;   // +60°

    // Smooth transition (仅在 Orbit 模式进入时使用)
    DirectX::XMFLOAT3 m_targetPosition{0, 0, -10.0f};  // 初始化与 m_position 一致
    float m_transitionSpeed = 5.0f; // lerp speed
    bool m_isTransitioning = false;  // 是否正在过渡动画

    // Camera switching
    CameraType m_cameraType = CameraType::FreeCam; // 当前摄像机类型
    CameraType m_targetCameraType = CameraType::FreeCam; // 目标摄像机类型
    float m_targetYaw = 0.0f; // 目标偏航角
    float m_targetPitch = 0.0f; // 目标俯仰角
    bool m_isSwitchingCamera = false; // 是否正在切换摄像机

    // Camera shake
    bool m_isShaking = false; // 是否正在抖动
    float m_shakeIntensity = 0.0f; // 抖动强度
    float m_shakeDuration = 0.0f; // 抖动持续时间
    float m_shakeTimer = 0.0f; // 抖动计时器
    DirectX::XMFLOAT3 m_shakeOffset{0, 0, 0}; // 当前抖动偏移量
};
