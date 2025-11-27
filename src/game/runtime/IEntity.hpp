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

    virtual EntityId id() const = 0;
    virtual Transform &transformRef() = 0;
    virtual std::span<ColliderBase *> colliders() = 0; // 只读访问裸指针集合
    virtual RigidBody *rigidBody() = 0;                 // 静态体返回 nullptr

    // 每帧逻辑入口（由场景在物理步后调用）
    virtual void update(WorldContext & /*ctx*/, float /*dt*/) {}

    // 触发器事件（场景从 PhysicsWorld 回调路由到具体实体）
    virtual void onTrigger(WorldContext & /*ctx*/, EntityId /*other*/, TriggerPhase /*phase*/, const OverlapResult & /*c*/) {}
};
