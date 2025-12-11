#pragma once
#pragma execution_character_set("utf-8")

#include <span>
#include <vector>
#include <memory>
#include <DirectXMath.h>

#include "../src/core/render/Drawable.hpp"
#include "../src/core/physics/Transform.hpp"
#include "../src/core/physics/RigidBody.hpp"
#include "../src/core/physics/Collider.hpp"
#include "../src/core/physics/PhysicsWorld.hpp"

struct WorldContext; // 前置声明

// 轻量实体接口：与现有 OOP 代码兼容，同时具备 ECS 风格的 update/onTrigger 钩子
struct IEntity : public IDrawable {
    virtual ~IEntity() = default;

    [[nodiscard]] virtual EntityId id() const = 0;

    virtual void setId(EntityId eid) = 0;

    virtual Transform &transformRef() = 0;

    virtual ColliderBase *collider() = 0; // 获取主 collider（最常用）
    virtual std::span<ColliderBase *> colliders() = 0; // 访问所有 colliders（用于复合碰撞体）
    virtual RigidBody *rigidBody() = 0; // 静态体返回 nullptr

    // 生命周期钩子：实体完全注册后调用（可访问其他实体、已分配ID）
    virtual void init(WorldContext & /*ctx*/) {
    }

    // 生命周期钩子：实体销毁前调用（仍在场景中、仍可访问其他实体）
    virtual void onDestroy(WorldContext & /*ctx*/) {
    }

    // 每帧逻辑入口（由场景在物理步后调用）
    virtual void update(WorldContext & /*ctx*/, float /*dt*/) {
    }

    // 碰撞/触发事件（场景从 PhysicsWorld 回调路由到具体实体）
    virtual void onCollision(WorldContext & /*ctx*/, EntityId /*other*/, TriggerPhase /*phase*/,
                             const OverlapResult & /*c*/) {
    }
};