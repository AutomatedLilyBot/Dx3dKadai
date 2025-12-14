#pragma once
#include <array>
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
        if (!modelRef || colliderCount_ == 0 || modelRef->empty()) return;
        auto* col = colliders_[0].get();

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

        switch (col->kind()) {
            case ColliderType::Sphere: {
                float r = (halfExt.x > halfExt.y ? halfExt.x : halfExt.y);
                r = (r > halfExt.z ? r : halfExt.z);
                col->setScale(DirectX::XMFLOAT3(r, r, r));
                col->setPosition(center);
                break;
            }
            case ColliderType::Obb:
                col->setScale(halfExt);
                col->setPosition(center);
                break;
            case ColliderType::Capsule: {
                float maxAxis = (halfExt.x > halfExt.y ? halfExt.x : halfExt.y);
                maxAxis = (maxAxis > halfExt.z ? maxAxis : halfExt.z);
                float r = (halfExt.y >= halfExt.x && halfExt.y >= halfExt.z)
                              ? (halfExt.x > halfExt.z ? halfExt.x : halfExt.z)
                              : ((halfExt.x >= halfExt.y && halfExt.x >= halfExt.z)
                                     ? (halfExt.y > halfExt.z ? halfExt.y : halfExt.z)
                                     : (halfExt.x > halfExt.y ? halfExt.x : halfExt.y));
                col->setScale(DirectX::XMFLOAT3(r, maxAxis, r));
                col->setPosition(center);
                break;
            }
        }
        col->updateDerived();
    }

    // IEntity
    EntityId id() const override { return id_; }
    Transform &transformRef() override { return transform; }

    // 直接返回主 collider（最常用的访问方式）
    ColliderBase *collider() override {
        return colliderCount_ > 0 ? colliders_[0].get() : nullptr;
    }

    // 返回所有 colliders 的 span（零拷贝，直接从缓存返回）
    std::span<ColliderBase *> colliders() override {
        // 延迟更新缓存：仅在碰撞体改变时重建
        if (colliderCacheDirty_) {
            for (size_t i = 0; i < colliderCount_; ++i) {
                cachedColPtrs_[i] = colliders_[i].get();
            }
            colliderCacheDirty_ = false;
        }
        return std::span<ColliderBase *>(cachedColPtrs_.data(), colliderCount_);
    }

    // ⚠️ 警告：替换主 Collider（仅在实体注册到物理世界之前调用）
    // 运行时替换会导致物理世界不同步！必须先 unregister 再 register
    // 推荐：在构造函数或 init() 中设置，之后不要修改
    void setCollider(std::unique_ptr<ColliderBase> col) {
        if (colliderCount_ == 0) {
            colliders_[0] = std::move(col);
            colliderCount_ = 1;
        } else {
            colliders_[0] = std::move(col);
        }
        colliderCacheDirty_ = true;
    }

    // 添加额外的碰撞体（用于复合碰撞体，如本体+触发器）
    // ⚠️ 同样需要在实体注册到物理世界之前调用
    void addCollider(std::unique_ptr<ColliderBase> col) {
        if (colliderCount_ < MaxColliders) {
            colliders_[colliderCount_++] = std::move(col);
            colliderCacheDirty_ = true;
        }
        // 超出容量时静默忽略（也可以添加日志或断言）
    }

    RigidBody *rigidBody() override { return nullptr; }

protected:
    EntityId id_ = 0;

    // 固定大小碰撞体数组（大部分实体只需要1-2个）
    static constexpr size_t MaxColliders = 4;
    std::array<std::unique_ptr<ColliderBase>, MaxColliders> colliders_;
    size_t colliderCount_ = 0;

    // 指针缓存：避免每次调用 colliders() 时重建
    mutable std::array<ColliderBase *, MaxColliders> cachedColPtrs_{};
    mutable bool colliderCacheDirty_ = false;

};
