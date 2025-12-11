#pragma once
#include <cmath>
#include <DirectXMath.h>

// Standalone Transform container
struct Transform {
    DirectX::XMFLOAT3 position{0, 0, 0};
    DirectX::XMFLOAT4 rotation{0, 0, 0, 1}; // quaternion (x, y, z, w)
    DirectX::XMFLOAT3 scale{1, 1, 1};

    DirectX::XMMATRIX world() const {
        using namespace DirectX;
        XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
        XMVECTOR quat = XMLoadFloat4(&rotation);
        XMMATRIX R = XMMatrixRotationQuaternion(quat);
        XMMATRIX T = XMMatrixTranslation(position.x, position.y, position.z);
        return S * R * T;
    }

    DirectX::XMMATRIX scaleMatrix() const {
        return DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);
    }

    DirectX::XMMATRIX rotationMatrix() const {
        using namespace DirectX;
        XMVECTOR quat = XMLoadFloat4(&rotation);
        return XMMatrixRotationQuaternion(quat);
    }

    DirectX::XMMATRIX translationMatrix() const {
        return DirectX::XMMatrixTranslation(position.x, position.y, position.z);
    }

    // 欧拉角接口（兼容性）
    void setRotationEuler(float pitch, float yaw, float roll) {
        using namespace DirectX;
        XMVECTOR quat = XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);
        XMStoreFloat4(&rotation, quat);
    }

    DirectX::XMFLOAT3 getRotationEuler() const {
        using namespace DirectX;
        XMVECTOR quat = XMLoadFloat4(&rotation);

        // 四元数转欧拉角 (Roll-Pitch-Yaw)
        float qx = rotation.x, qy = rotation.y, qz = rotation.z, qw = rotation.w;

        // Pitch (X-axis)
        float sinp = 2.0f * (qw * qx - qy * qz);
        float pitch;
        if (std::abs(sinp) >= 1.0f)
            pitch = std::copysign(DirectX::XM_PIDIV2, sinp); // ±90°
        else
            pitch = std::asin(sinp);

        // Yaw (Y-axis)
        float siny_cosp = 2.0f * (qw * qy + qx * qz);
        float cosy_cosp = 1.0f - 2.0f * (qx * qx + qy * qy);
        float yaw = std::atan2(siny_cosp, cosy_cosp);

        // Roll (Z-axis)
        float sinr_cosp = 2.0f * (qw * qz + qx * qy);
        float cosr_cosp = 1.0f - 2.0f * (qx * qx + qz * qz);
        float roll = std::atan2(sinr_cosp, cosr_cosp);

        return DirectX::XMFLOAT3{pitch, yaw, roll};
    }

    void translateWorld(float dx, float dy, float dz) {
        position.x += dx;
        position.y += dy;
        position.z += dz;
    }

    void rotateEuler(float dPitch, float dYaw, float dRoll) {
        using namespace DirectX;
        XMVECTOR deltaQuat = XMQuaternionRotationRollPitchYaw(dPitch, dYaw, dRoll);
        XMVECTOR currentQuat = XMLoadFloat4(&rotation);
        XMVECTOR newQuat = XMQuaternionMultiply(currentQuat, deltaQuat);
        XMStoreFloat4(&rotation, XMQuaternionNormalize(newQuat));
    }

    void translateLocal(float dx, float dy, float dz) {
        using namespace DirectX;
        XMMATRIX rotM = rotationMatrix();
        XMVECTOR deltaLocal = XMVectorSet(dx, dy, dz, 0.0f);
        XMVECTOR deltaWorld = XMVector3TransformNormal(deltaLocal, rotM);
        XMVECTOR pos = XMLoadFloat3(&position);
        pos = XMVectorAdd(pos, deltaWorld);
        XMStoreFloat3(&position, pos);
    }

    void lookAt(const DirectX::XMFLOAT3 &target,
                const DirectX::XMFLOAT3 &worldUp = DirectX::XMFLOAT3{0.0f, 1.0f, 0.0f}) {
        using namespace DirectX;
        XMVECTOR eyeV = XMLoadFloat3(&position);
        XMVECTOR atV = XMLoadFloat3(&target);
        XMVECTOR upV = XMLoadFloat3(&worldUp);

        // 计算观察矩阵，然后转换为旋转四元数
        XMMATRIX viewMatrix = XMMatrixLookAtLH(eyeV, atV, upV);
        // LookAt 生成的是 view 矩阵，需要求逆得到 world rotation
        XMVECTOR det;
        XMMATRIX rotMatrix = XMMatrixInverse(&det, viewMatrix);
        // 只取旋转部分（去掉平移）
        rotMatrix.r[3] = XMVectorSet(0, 0, 0, 1);

        XMVECTOR quat = XMQuaternionRotationMatrix(rotMatrix);
        XMStoreFloat4(&rotation, XMQuaternionNormalize(quat));
    }


    void moveTowards(const DirectX::XMFLOAT3 &target, float maxDistanceDelta) {
        using namespace DirectX;
        XMVECTOR posV = XMLoadFloat3(&position);
        XMVECTOR tgtV = XMLoadFloat3(&target);
        XMVECTOR delta = XMVectorSubtract(tgtV, posV);
        float dist = XMVectorGetX(XMVector3Length(delta));
        if (dist <= maxDistanceDelta || dist <= 1e-6f) {
            position = target;
            return;
        }
        float t = maxDistanceDelta / dist;
        XMVECTOR newPos = XMVectorLerp(posV, tgtV, t);
        XMStoreFloat3(&position, newPos);
    }

    // 插值两个 Transform（用于动画）
    static Transform lerp(const Transform &a, const Transform &b, float t) {
        using namespace DirectX;
        Transform r;

        // 位置线性插值
        XMVECTOR posA = XMLoadFloat3(&a.position);
        XMVECTOR posB = XMLoadFloat3(&b.position);
        XMStoreFloat3(&r.position, XMVectorLerp(posA, posB, t));

        // 旋转球面插值 (Slerp)
        XMVECTOR quatA = XMLoadFloat4(&a.rotation);
        XMVECTOR quatB = XMLoadFloat4(&b.rotation);
        XMStoreFloat4(&r.rotation, XMQuaternionSlerp(quatA, quatB, t));

        // 缩放线性插值
        XMVECTOR scaleA = XMLoadFloat3(&a.scale);
        XMVECTOR scaleB = XMLoadFloat3(&b.scale);
        XMStoreFloat3(&r.scale, XMVectorLerp(scaleA, scaleB, t));

        return r;
    }
};