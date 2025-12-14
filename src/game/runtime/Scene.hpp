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

        // 3) 构建实体查询接口并填充 WorldContext
        EntityQuery entityQuery{};
        entityQuery.entityMap = &id2ptr_;

        WorldContext ctx{};
        ctx.time = time_;
        ctx.dt = dt;
        ctx.physics = &query_;
        ctx.entities = &entityQuery;
        ctx.commands = &cmdBuffer_;

        // 3.1) 派发 onCollision
        for (const auto &ev: frameCollisionEvents_) {
            auto it = id2ptr_.find(ev.a);
            if (it != id2ptr_.end() && it->second) {
                it->second->onCollision(ctx, ev.b, ev.phase, ev.contact);
            }
        }
        frameCollisionEvents_.clear();

        // 3.2) 逐实体 update
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

protected:
    // 子类可用的工具方法
    void setupPhysicsCallback() {
        world_.setTriggerCallback([this](EntityId a, EntityId b, TriggerPhase phase, const OverlapResult &contact) {
            // 1) 缓存碰撞事件（双向）
            tempCollisionEvents_.push_back(CollisionEvent{a, b, phase, contact});
            tempCollisionEvents_.push_back(CollisionEvent{b, a, phase, contact});

            // 2) 维护触发器重叠视图
            auto entityHasTrigger = [this](EntityId e) -> bool {
                auto it = id2ptr_.find(e);
                if (it == id2ptr_.end() || !it->second) return false;
                auto span = it->second->colliders();
                for (auto *c: span) { if (c && c->isTrigger()) return true; }
                return false;
            };
            bool isTriggerInvolved = entityHasTrigger(a) || entityHasTrigger(b);

            auto addPair = [&](EntityId ta, EntityId tb) {
                tempTriggerOverlaps_[ta].insert(tb);
            };
            auto removePair = [&](EntityId ta, EntityId tb) {
                auto it = triggerOverlaps_.find(ta);
                if (it != triggerOverlaps_.end()) it->second.erase(tb);
                auto it2 = tempTriggerOverlaps_.find(ta);
                if (it2 != tempTriggerOverlaps_.end()) it2->second.erase(tb);
            };

            if (isTriggerInvolved) {
                if (phase == TriggerPhase::Exit) {
                    removePair(a, b);
                    removePair(b, a);
                } else {
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