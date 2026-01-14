#pragma once
#include "StaticEntity.hpp"
#include "../../core/gfx/Camera.hpp"
#include <DirectXMath.h>
#include "game/runtime/WorldContext.hpp"

// Billboard 朝向类型
enum class BillboardType {
    FullBillboard, // 完全面向相机（爆炸特效）
    YAxisBillboard, // 仅绕Y轴旋转（立牌、树木）
    None // 不自动朝向（固定方向）
};

// Billboard 实体基类
// 用于始终面向相机的平面效果（特效、UI元素等）
class BillboardEntity : public StaticEntity {
public:
    BillboardType orientationType = BillboardType::FullBillboard;

    // 生命周期控制
    float lifetime = 0.0f; // 生存时间（0表示永久）
    float currentTime = 0.0f; // 当前存活时间
    bool autoDestroy = false; // 是否自动销毁

    // UV 动画支持
    DirectX::XMFLOAT2 uvOffset = {0.0f, 0.0f}; // UV偏移（用于滚动动画）
    DirectX::XMFLOAT2 uvScale = {1.0f, 1.0f}; // UV缩放
    DirectX::XMFLOAT2 uvScrollSpeed = {0.0f, 0.0f}; // UV滚动速度

    // 序列帧动画支持（可选）
    int frameCount = 1; // 总帧数
    int currentFrame = 0; // 当前帧索引
    float frameTime = 0.0f; // 每帧持续时间
    float frameTimer = 0.0f; // 当前帧计时器

    // 更新逻辑
    void update(WorldContext &ctx, float dt) override {
        // 更新生命周期
        if (autoDestroy && lifetime > 0.0f) {
            currentTime += dt;
            if (currentTime >= lifetime) {
                ctx.commands->destroyEntity(id());
                return;
            }
        }

        // 更新UV滚动
        if (uvScrollSpeed.x != 0.0f || uvScrollSpeed.y != 0.0f) {
            uvOffset.x += uvScrollSpeed.x * dt;
            uvOffset.y += uvScrollSpeed.y * dt;
            // 可选：循环UV到[0,1]范围
            uvOffset.x = fmodf(uvOffset.x, 1.0f);
            uvOffset.y = fmodf(uvOffset.y, 1.0f);
        }

        // 更新序列帧动画
        if (frameCount > 1 && frameTime > 0.0f) {
            frameTimer += dt;
            if (frameTimer >= frameTime) {
                frameTimer = 0.0f;
                currentFrame = (currentFrame + 1) % frameCount;
                // 根据当前帧更新UV（假设是横向排列的序列帧）
                float frameWidth = 1.0f / frameCount;
                uvOffset.x = frameWidth * currentFrame;
                uvScale.x = frameWidth;
            }
        }
    }

    bool isTransparent() const override { return true; }

    float getAlpha() const override { return materialData.baseColorFactor.w; }

    // 更新朝向矩阵（在渲染前调用）
    void updateBillboardOrientation(const Camera *camera) {
        if (!camera || orientationType == BillboardType::None) {
            return;
        }

        using namespace DirectX;
        XMFLOAT3 camPos = camera->getPosition();
        XMFLOAT3 billboardPos = transform.position;

        if (orientationType == BillboardType::FullBillboard) {
            // 完全朝向相机
            XMVECTOR toCamera = XMVectorSubtract(
                XMLoadFloat3(&camPos),
                XMLoadFloat3(&billboardPos)
            );
            toCamera = XMVector3Normalize(toCamera);

            // 构造朝向相机的旋转矩阵
            XMVECTOR up = XMVectorSet(0, 1, 0, 0);
            XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, toCamera));
            up = XMVector3Cross(toCamera, right);

            // 将旋转存储到 transform
            XMMATRIX rotMat;
            rotMat.r[0] = XMVectorSetW(right, 0);
            rotMat.r[1] = XMVectorSetW(up, 0);
            rotMat.r[2] = XMVectorSetW(toCamera, 0);
            rotMat.r[3] = XMVectorSet(0, 0, 0, 1);

            // 从旋转矩阵提取欧拉角（简化版，仅用于存储）
            // 注意：这里不需要精确的欧拉角，因为我们会在 world() 中直接使用矩阵
            cachedRotationMatrix = rotMat;
            useCachedRotation = true;
        } else if (orientationType == BillboardType::YAxisBillboard) {
            // 仅绕Y轴旋转
            XMFLOAT3 toCamera = {
                camPos.x - billboardPos.x,
                0.0f, // 忽略Y轴差异
                camPos.z - billboardPos.z
            };
            XMVECTOR dir = XMLoadFloat3(&toCamera);
            dir = XMVector3Normalize(dir);

            // 计算绕Y轴的旋转角度
            float yaw = atan2f(XMVectorGetX(dir), XMVectorGetZ(dir));
            transform.setRotationEuler(0.0f, yaw, 0.0f);
            useCachedRotation = false;
        }
    }

    // 重写 world() 方法，使用缓存的旋转矩阵（用于 FullBillboard）
    DirectX::XMMATRIX world() const override {
        using namespace DirectX;
        if (useCachedRotation) {
            return transform.scaleMatrix() * modelBias.world() *
                   cachedRotationMatrix * transform.translationMatrix();
        } else {
            return StaticEntity::world();
        }
    }

protected:
    bool useCachedRotation = false;
    DirectX::XMMATRIX cachedRotationMatrix;
};
