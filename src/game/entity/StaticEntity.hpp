#pragma once
#include <memory>
#include <utility>
#include <vector>
#include <DirectXMath.h>
#include "../src/core/physics/Transform.hpp"
#include "../../core/physics/Collider.hpp"
#include "../src/core/render/Drawable.hpp"
#include "../runtime/IEntity.hpp"

// Optional behaviour placeholder
struct IBallAffecter {
    virtual ~IBallAffecter() = default;

    virtual void applyForces(float /*dt*/) {
    }
};

// 基础静态实体：具备 Transform + 可选 Model + 可选单个 Collider
// 现在实现 IEntity 接口以接入 Scene 的调度与 PhysicsWorld 的注册
class StaticEntity : public IEntity /*, public IBallAffecter */ {
public:
    // 公共数据
    Transform transform;
    Transform modelBias; // Model 专用的变换偏移（用于视觉与逻辑解耦）
    const Model *modelRef = nullptr; // 指向外部持有的模型
    Material materialData; // 简化的材质


    // 身份标识由上层（Scene/EntityManager）分配
    void setId(EntityId eid) { id_ = eid; }

    // IDrawable
    // 变换顺序：Entity.S * ModelBias.SRT * Entity.RT
    // Entity.S 先应用（让模型沿世界轴缩放），再应用 modelBias"摆正"姿态，最后应用 Entity.RT 定位
    DirectX::XMMATRIX world() const override {
        using namespace DirectX;
        return transform.scaleMatrix() * modelBias.world() *
               transform.rotationMatrix() * transform.translationMatrix();
    }

    const Model *model() const override { return modelRef; }
    const Material *material() const override { return &materialData; }

    // 根据 Model 自动调整 Collider 大小（仅在单个 Collider 时有效）
    void fitColliderToModel() {
        if (!modelRef || !collider_ || modelRef->empty()) return;

        // Compute AABB from all mesh vertices (in model's local space)
        // 注意：不应用 nodeGlobal 和 modelBias 变换，只使用原始顶点数据
        using namespace DirectX;
        constexpr float fmax = 3.402823466e+38F; // FLT_MAX
        constexpr float fmin = -3.402823466e+38F;
        XMFLOAT3 minB(fmax, fmax, fmax);
        XMFLOAT3 maxB(fmin, fmin, fmin);

        bool hasVertices = false;

        for (const auto &drawItem: modelRef->drawItems) {
            if (drawItem.meshIndex >= modelRef->meshes.size()) continue;
            const auto &meshGPU = modelRef->meshes[drawItem.meshIndex];
            const auto &verts = meshGPU.mesh.vertices();

            if (verts.empty()) continue;
            hasVertices = true;

            // 应用 modelBias 变换（如果有的话）
            // 这样collider才能匹配渲染后的视觉效果
            XMMATRIX modelBiasMatrix = modelBias.world();
            bool hasModelBias = (!XMVector4Equal(modelBiasMatrix.r[0], XMVectorSet(1, 0, 0, 0)) || !XMVector4Equal(
                                     modelBiasMatrix.r[1],
                                     XMVectorSet(0, 1, 0, 0)) || !XMVector4Equal(
                                     modelBiasMatrix.r[2], XMVectorSet(0, 0, 1, 0)) || !XMVector4Equal(
                                     modelBiasMatrix.r[3], XMVectorSet(0, 0, 0, 1)));

            for (const auto &v: verts) {
                XMVECTOR pos = XMLoadFloat3(&v.pos);

                // 如果有modelBias，应用它
                if (hasModelBias) {
                    pos = XMVector3TransformCoord(pos, modelBiasMatrix);
                }

                XMFLOAT3 p;
                XMStoreFloat3(&p, pos);

                minB.x = (p.x < minB.x) ? p.x : minB.x;
                minB.y = (p.y < minB.y) ? p.y : minB.y;
                minB.z = (p.z < minB.z) ? p.z : minB.z;

                maxB.x = (p.x > maxB.x) ? p.x : maxB.x;
                maxB.y = (p.y > maxB.y) ? p.y : maxB.y;
                maxB.z = (p.z > maxB.z) ? p.z : maxB.z;
            }
        }

        // Fallback: if no vertices found, use unit cube
        if (!hasVertices) {
            minB = XMFLOAT3(-0.5f, -0.5f, -0.5f);
            maxB = XMFLOAT3(0.5f, 0.5f, 0.5f);
        }

        XMFLOAT3 center((minB.x + maxB.x) * 0.5f,
                        (minB.y + maxB.y) * 0.5f,
                        (minB.z + maxB.z) * 0.5f);
        XMFLOAT3 halfExt((maxB.x - minB.x) * 0.5f,
                         (maxB.y - minB.y) * 0.5f,
                         (maxB.z - minB.z) * 0.5f);

        switch (collider_->kind()) {
            case ColliderType::Sphere: {
                float r = (halfExt.x > halfExt.y ? halfExt.x : halfExt.y);
                r = (r > halfExt.z ? r : halfExt.z);
                collider_->setScale(DirectX::XMFLOAT3(r, r, r));
                collider_->setPosition(center);
                break;
            }
            case ColliderType::Obb:
                collider_->setScale(halfExt);
                collider_->setPosition(center);
                break;
            case ColliderType::Capsule: {
                float maxAxis = (halfExt.x > halfExt.y ? halfExt.x : halfExt.y);
                maxAxis = (maxAxis > halfExt.z ? maxAxis : halfExt.z);
                float r = (halfExt.y >= halfExt.x && halfExt.y >= halfExt.z)
                              ? (halfExt.x > halfExt.z ? halfExt.x : halfExt.z)
                              : ((halfExt.x >= halfExt.y && halfExt.x >= halfExt.z)
                                     ? (halfExt.y > halfExt.z ? halfExt.y : halfExt.z)
                                     : (halfExt.x > halfExt.y ? halfExt.x : halfExt.y));
                collider_->setScale(DirectX::XMFLOAT3(r, maxAxis, r));
                collider_->setPosition(center);
                break;
            }
        }
        collider_->updateDerived();
    }

    // IEntity
    EntityId id() const override { return id_; }
    Transform &transformRef() override { return transform; }

    // 直接返回主 collider（最常用的访问方式）
    ColliderBase *collider() override { return collider_.get(); }

    // 返回所有 colliders 的 span（用于兼容性）
    std::span<ColliderBase *> colliders() override {
        if (!collider_) return {};
        cachedColPtr_ = collider_.get();
        return std::span<ColliderBase *>(&cachedColPtr_, 1);
    }

    // ⚠️ 警告：替换 Collider（仅在实体注册到物理世界之前调用）
    // 运行时替换会导致物理世界不同步！必须先 unregister 再 register
    // 推荐：在构造函数或 init() 中设置，之后不要修改
    void setCollider(std::unique_ptr<ColliderBase> col) {
        collider_ = std::move(col);
    }

    RigidBody *rigidBody() override { return nullptr; }

protected:
    EntityId id_ = 0;
    ColliderBase *cachedColPtr_ = nullptr; // 用于返回 span 的稳定指针
    // Collider 访问受保护：防止运行时直接替换导致物理世界不同步
    // 使用 collider() 方法访问，或在子类构造函数中初始化
    std::unique_ptr<ColliderBase> collider_;

};
