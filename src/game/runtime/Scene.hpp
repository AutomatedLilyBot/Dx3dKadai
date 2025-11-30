#pragma once
#pragma execution_character_set("utf-8")

#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "WorldContext.hpp"
#include "IEntity.hpp"
#include "../src/core/gfx/Renderer.hpp"
#include "../src/core/physics/PhysicsWorld.hpp"
#include "../src/core/physics/Transform.hpp"

// 最小场景调度器：
// - 驱动物理步
// - 构建（占位）PhysicsQuery 视图
// - 遍历实体并调用 update（当前无实体也可运行）
class Scene {
public:
    Scene() = default;

    void init(Renderer *renderer);

    // 每帧更新（不负责渲染，渲染由上层调用 Renderer 完成）
    void tick(float dt);

    // 渲染场景（最小演示：绘制一个内部创建的 cube）
    void render();

    // 访问底层 PhysicsWorld（后续可移除）
    PhysicsWorld &physics() { return world_; }

private:
    void buildPhysicsQueryFromLastFrame();

    void submitCommands();

    // 实体管理
    EntityId allocId() { return ++nextId_; }

    void registerEntity(IEntity &e);

    void unregisterEntity(EntityId id);

private:
    Renderer *renderer_ = nullptr; // 仅保存指针，生命周期由外部管理
    PhysicsWorld world_{};
    PhysicsQuery query_{}; // 当前帧只读物理查询视图（占位）
    CommandBuffer cmdBuffer_{}; // 命令缓冲（占位）
    float time_ = 0.0f;

    // Demo: 一个简单的 cube 变换，用于验证 render 调度
    Transform demoCubeTransform_{};

    // 资源与实体容器
    std::vector<std::unique_ptr<IEntity> > entities_;
    std::unordered_map<EntityId, IEntity *> id2ptr_;
    EntityId nextId_ = 0;

    // 触发器重叠缓存（entity→set），由物理回调维护
    std::unordered_map<EntityId, std::unordered_set<EntityId> > triggerOverlaps_;
    std::unordered_map<EntityId, std::unordered_set<EntityId> > tempTriggerOverlaps_;
};
