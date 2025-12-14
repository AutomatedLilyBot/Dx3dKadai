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

// 抽象场景基类：
// - 驱动物理步
// - 构建 PhysicsQuery 视图
// - 注册/注销实体，提交/消化命令
// - 创建物理回调派发物理事件
// 子类需实现 init() 来创建具体场景内容
class Scene {
public:
    Scene() = default;
    virtual ~Scene() = default;

    // 场景初始化（纯虚函数，子类实现具体场景内容）
    virtual void init(Renderer *renderer) = 0;

    // 每帧更新
    void tick(float dt) {
        time_ += dt;

        // 0.5) 物理前：将外部直接改动的 Transform 写入 PhysicsWorld
        for (auto &ptr: entities_) {
            if (!ptr) continue;
            const auto &tr = ptr->transformRef();
            DirectX::XMFLOAT3 rotEuler = tr.getRotationEuler();
            world_.syncOwnerTransform(ptr->id(), tr.position, rotEuler, false);
        }

        // 1) 物理步
        world_.step(dt);

        // 2) 构建只读物理查询视图
        buildPhysicsQueryFromLastFrame();

        // 2.5) 将物理解算后的刚体位置写回实体 Transform
        for (auto &ptr: entities_) {
            if (!ptr) continue;
            if (RigidBody *rb = ptr->rigidBody()) {
                ptr->transformRef().position = rb->position;
            }
        }

        // === 3) 构建 WorldContext 并派发事件到实体 ===
        // 准备只读查询接口
        EntityQuery entityQuery{};
        entityQuery.entityMap = &id2ptr_;

        // 构建上下文：提供给所有实体的只读物理查询、实体查询和命令缓冲
        WorldContext ctx{};
        ctx.time = time_;
        ctx.dt = dt;
        ctx.physics = &query_;       // 物理查询（包含触发器重叠状态）
        ctx.entities = &entityQuery; // 实体查询
        ctx.commands = &cmdBuffer_;  // 命令缓冲（用于生成/销毁实体）
        ctx.resources = getResourceManager(); // 资源管理器（子类提供）

        // 3.1) 派发碰撞/触发事件到实体
        // 遍历本帧所有碰撞事件，调用实体的 onCollision 回调
        // 注意：
        // - 所有碰撞都会触发此回调（无论是否涉及触发器）
        // - 触发器碰撞不会产生物理响应，但仍然会收到事件通知
        // - 实体可以通过 collider->isTrigger() 判断碰撞类型
        for (const auto &ev: frameCollisionEvents_) {
            auto it = id2ptr_.find(ev.a);
            if (it != id2ptr_.end() && it->second) {
                it->second->onCollision(ctx, ev.b, ev.phase, ev.contact);
            }
        }
        frameCollisionEvents_.clear();

        // 3.2) 逐实体更新逻辑
        // 调用每个实体的 update() 方法，实体可以：
        // - 查询物理状态（如触发器重叠）
        // - 修改自身状态
        // - 通过 ctx.commands 发送生成/销毁命令
        for (auto &ptr: entities_) {
            if (ptr) ptr->update(ctx, dt);
        }

        // 4) 提交命令缓冲
        submitCommands();
    }

    // 渲染场景
    void render() {
        if (!renderer_) return;
        for (auto &ptr: entities_) {
            if (!ptr) continue;
            if (ptr->model()) {
                renderer_->draw(*ptr);
            }
            auto span = ptr->colliders();
            std::vector<ColliderBase *> cols(span.begin(), span.end());
            if (!cols.empty()) {
                for (auto *c: cols) {
                    renderer_->drawColliderWire(*c);
                }
            }
        }
    }

    // 访问底层 PhysicsWorld
    PhysicsWorld &physics() { return world_; }

    // 访问 Renderer（用于实体工厂）
    Renderer *renderer() { return renderer_; }

    // 访问 ResourceManager（子类需要实现，返回其持有的 ResourceManager）
    virtual ResourceManager *getResourceManager() { return nullptr; }

    // 输入处理接口（子类实现具体的交互逻辑）
    virtual void handleInput(float dt, const void* window) {}

    // 获取实体映射表（用于 InputManager 射线检测）
    const std::unordered_map<EntityId, IEntity*>* getEntityMap() const { return &id2ptr_; }

protected:
    // 子类可用的工具方法
    // 设置物理世界的碰撞/触发回调
    // 此回调在物理步进时由 PhysicsWorld 调用，用于：
    // 1. 收集所有碰撞事件，稍后分发给实体的 onCollision()
    // 2. 维护触发器重叠状态映射，供 PhysicsQuery 查询
    void setupPhysicsCallback() {
        world_.setTriggerCallback([this](EntityId a, EntityId b, TriggerPhase phase, const OverlapResult &contact) {
            // === 步骤 1：缓存碰撞事件（用于后续分发给实体） ===
            // 注意：事件是双向的，A 和 B 都会收到 onCollision 回调
            // - 实体 A 收到 onCollision(ctx, B, phase, contact)
            // - 实体 B 收到 onCollision(ctx, A, phase, contact)
            tempCollisionEvents_.push_back(CollisionEvent{a, b, phase, contact});
            tempCollisionEvents_.push_back(CollisionEvent{b, a, phase, contact});

            // === 步骤 2：维护触发器重叠状态映射 ===
            // 检查碰撞对中是否有触发器碰撞体
            auto entityHasTrigger = [this](EntityId e) -> bool {
                auto it = id2ptr_.find(e);
                if (it == id2ptr_.end() || !it->second) return false;
                auto span = it->second->colliders();
                for (auto *c: span) { if (c && c->isTrigger()) return true; }
                return false;
            };

            // 只有当至少一方拥有触发器碰撞体时，才记录到触发器重叠映射
            // 这样可以过滤掉纯物理碰撞（两方都是实体碰撞体）
            bool isTriggerInvolved = entityHasTrigger(a) || entityHasTrigger(b);

            // 辅助函数：添加触发器重叠对
            auto addPair = [&](EntityId ta, EntityId tb) {
                tempTriggerOverlaps_[ta].insert(tb);
            };

            // 辅助函数：移除触发器重叠对（从当前帧和临时缓存中移除）
            auto removePair = [&](EntityId ta, EntityId tb) {
                auto it = triggerOverlaps_.find(ta);
                if (it != triggerOverlaps_.end()) it->second.erase(tb);
                auto it2 = tempTriggerOverlaps_.find(ta);
                if (it2 != tempTriggerOverlaps_.end()) it2->second.erase(tb);
            };

            if (isTriggerInvolved) {
                if (phase == TriggerPhase::Exit) {
                    // 触发器退出：从双向映射中移除
                    removePair(a, b);
                    removePair(b, a);
                } else {
                    // 触发器进入或保持：添加到双向映射
                    // phase 可以是 Enter 或 Stay
                    addPair(a, b);
                    addPair(b, a);
                }
            }
        });
    }

    void buildPhysicsQueryFromLastFrame() {
        triggerOverlaps_ = tempTriggerOverlaps_;
        tempTriggerOverlaps_.clear();
        query_.triggerOverlaps = &triggerOverlaps_;
        frameCollisionEvents_ = std::move(tempCollisionEvents_);
        tempCollisionEvents_.clear();
    }

    void submitCommands() {
        // Reset 优先
        if (cmdBuffer_.reset.doReset) {
            for (size_t i = 0; i < entities_.size();) {
                auto *e = entities_[i].get();
                if (e && e->rigidBody() != nullptr) {
                    unregisterEntity(e->id());
                    id2ptr_.erase(e->id());
                    entities_.erase(entities_.begin() + i);
                    continue;
                }
                ++i;
            }
            cmdBuffer_.clear();
            return;
        }

        // 销毁命令
        for (auto &dc: cmdBuffer_.toDestroy) {
            auto it = id2ptr_.find(dc.id);
            if (it != id2ptr_.end()) {
                WorldContext destroyCtx{};
                destroyCtx.time = time_;
                destroyCtx.dt = 0.0f;
                EntityQuery entityQueryForDestroy{};
                entityQueryForDestroy.entityMap = &id2ptr_;
                destroyCtx.physics = &query_;
                destroyCtx.entities = &entityQueryForDestroy;
                destroyCtx.commands = &cmdBuffer_;
                it->second->onDestroy(destroyCtx);

                unregisterEntity(dc.id);

                for (size_t i = 0; i < entities_.size(); ++i) {
                    if (entities_[i].get() == it->second) {
                        entities_.erase(entities_.begin() + i);
                        break;
                    }
                }
                id2ptr_.erase(it);
            }
        }

        // 通用实体生成命令
        for (auto &cmd: cmdBuffer_.spawnEntities) {
            auto entity = cmd.factory(this);
            if (!entity) continue;

            entity->setId(allocId());

            if (cmd.configurator) {
                cmd.configurator(entity.get());
            }

            registerEntity(*entity);
            EntityId eid = entity->id();
            IEntity *entityPtr = entity.get();
            id2ptr_[eid] = entityPtr;
            entities_.push_back(std::move(entity));

            WorldContext initCtx{};
            initCtx.time = time_;
            initCtx.dt = 0.0f;
            EntityQuery entityQueryForInit{};
            entityQueryForInit.entityMap = &id2ptr_;
            initCtx.physics = &query_;
            initCtx.entities = &entityQueryForInit;
            initCtx.commands = &cmdBuffer_;
            entityPtr->init(initCtx);
        }

        cmdBuffer_.clear();
    }

    // 实体管理
    EntityId allocId() { return ++nextId_; }

    void registerEntity(IEntity &e) {
        auto span = e.colliders();
        std::vector<ColliderBase *> cols(span.begin(), span.end());
        if (!cols.empty()) {
            const DirectX::XMFLOAT3 pos = e.transformRef().position;
            const DirectX::XMFLOAT3 rot = e.transformRef().getRotationEuler();
            for (auto *c: cols) {
                if (!c) continue;
                c->setOwnerWorldPosition(pos);
                c->setOwnerWorldRotationEuler(rot);
                c->updateDerived();
            }
        }
        RigidBody *rb = e.rigidBody();
        world_.registerEntity(e.id(), rb, std::span<ColliderBase *>(cols.data(), cols.size()));
    }

    void unregisterEntity(EntityId id) {
        world_.unregisterEntity(id);
    }

protected:
    Renderer *renderer_ = nullptr; // 仅保存指针，生命周期由外部管理
    PhysicsWorld world_{};
    PhysicsQuery query_{}; // 当前帧只读物理查询视图
    CommandBuffer cmdBuffer_{}; // 命令缓冲
    float time_ = 0.0f;

    // 资源与实体容器
    std::vector<std::unique_ptr<IEntity> > entities_;
    std::unordered_map<EntityId, IEntity *> id2ptr_;
    EntityId nextId_ = 0;

    // 触发器重叠缓存（entity→set），由物理回调维护
    std::unordered_map<EntityId, std::unordered_set<EntityId> > triggerOverlaps_;
    std::unordered_map<EntityId, std::unordered_set<EntityId> > tempTriggerOverlaps_;

    // 碰撞事件缓存（物理回调 → 帧内派发）
    struct CollisionEvent {
        EntityId a{0};
        EntityId b{0};
        TriggerPhase phase{TriggerPhase::Stay};
        OverlapResult contact{};
    };

    std::vector<CollisionEvent> tempCollisionEvents_;
    std::vector<CollisionEvent> frameCollisionEvents_;
};

// CommandBuffer::spawnBall 实现（需要在 Scene 定义之后）
#include "game/entity/BallEntity.hpp"
#include <windows.h>

inline void CommandBuffer::spawnBall(const DirectX::XMFLOAT3 &pos, float radius) {
    SpawnEntityCmd cmd;

    // 工厂函数：创建 BallEntity
    cmd.factory = [radius](Scene *scene) -> std::unique_ptr<IEntity> {
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        std::wstring exePath(buf);
        auto lastSlash = exePath.find_last_of(L"\\/");
        std::wstring exeDir = (lastSlash == std::wstring::npos) ? L"." : exePath.substr(0, lastSlash);

        return std::make_unique<BallEntity>(
            scene->renderer()->device(),
            radius,
            exeDir + L"\\asset\\ball.fbx"
        );
    };

    // 配置函数：设置位置等属性
    cmd.configurator = [pos, radius](IEntity *e) {
        auto *ball = static_cast<BallEntity *>(e);
        ball->transform.position = pos;
        ball->transform.scale = {radius, radius, radius};
        ball->rb.position = pos;
        ball->collider()->setDebugEnabled(true);
        ball->collider()->setDebugColor(DirectX::XMFLOAT4(0, 1, 0, 1));
    };

    spawnEntities.push_back(std::move(cmd));
}