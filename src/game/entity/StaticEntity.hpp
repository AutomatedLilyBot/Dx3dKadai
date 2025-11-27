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
    virtual void applyForces(float /*dt*/) {}
};

// 基础静态实体：具备 Transform + 可选 Model + 可选单个 Collider
// 现在实现 IEntity 接口以接入 Scene 的调度与 PhysicsWorld 的注册
class StaticEntity : public IEntity /*, public IBallAffecter */ {
public:
    // 数据
    Transform transform;
    std::unique_ptr<ColliderBase> collider; // 可为空；若存在，默认不是 trigger
    const Model *modelRef = nullptr;        // 指向外部持有的模型
    Material materialData;                  // 简化的材质

    // 身份标识由上层（Scene/EntityManager）分配
    void setId(EntityId eid) { id_ = eid; }

    // IDrawable
    DirectX::XMMATRIX world() const override { return transform.world(); }
    const Model *model() const override { return modelRef; }
    const Material *material() const override { return &materialData; }

    // IEntity
    EntityId id() const override { return id_; }
    Transform &transformRef() override { return transform; }
    std::span<ColliderBase *> colliders() override {
        rawCols_.clear();
        if (collider) rawCols_.push_back(collider.get());
        return std::span<ColliderBase *>(rawCols_.data(), rawCols_.size());
    }
    RigidBody *rigidBody() override { return nullptr; }

protected:
    EntityId id_ = 0;
    // 用于返回 span 的临时缓存
    std::vector<ColliderBase *> rawCols_{};
};
