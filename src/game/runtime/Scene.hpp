#pragma once
#include <algorithm>
#pragma execution_character_set("utf-8")
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include "WorldContext.hpp"
#include "IEntity.hpp"
#include "../src/core/gfx/Renderer.hpp"
#include "../src/core/physics/PhysicsWorld.hpp"
#include "../src/core/physics/Transform.hpp"
#include "core/resource/ResourceManager.hpp"
#include "game/ui/UIElement.hpp"

class SceneManager;

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
    virtual void tick(float dt) {
        time_ += dt;

        // 性能计时开始
        auto frameStart = std::chrono::high_resolution_clock::now();
        auto lastCheckpoint = frameStart;

        // 0.5) 物理前：将外部直接改动的 Transform 写入 PhysicsWorld
        for (auto &ptr: entities_) {
            if (!ptr) continue;
            const auto &tr = ptr->transformRef();
            DirectX::XMFLOAT3 rotEuler = tr.getRotationEuler();
            world_.syncOwnerTransform(ptr->id(), tr.position, rotEuler, false);
        }

        auto checkpoint1 = std::chrono::high_resolution_clock::now();
        float syncTransformTime = std::chrono::duration<float, std::milli>(checkpoint1 - lastCheckpoint).count();
        lastCheckpoint = checkpoint1;

        // 1) 物理步
        world_.step(dt);

        auto checkpoint2 = std::chrono::high_resolution_clock::now();
        float physicsStepTime = std::chrono::duration<float, std::milli>(checkpoint2 - lastCheckpoint).count();
        lastCheckpoint = checkpoint2;

        // 2) 构建只读物理查询视图
        buildPhysicsQueryFromLastFrame();

        auto checkpoint3 = std::chrono::high_resolution_clock::now();
        float buildQueryTime = std::chrono::duration<float, std::milli>(checkpoint3 - lastCheckpoint).count();
        lastCheckpoint = checkpoint3;

        // 2.5) 将物理解算后的刚体位置写回实体 Transform
        for (auto &ptr: entities_) {
            if (!ptr) continue;
            if (RigidBody *rb = ptr->rigidBody()) {
                ptr->transformRef().position = rb->position;
            }
        }

        auto checkpoint4 = std::chrono::high_resolution_clock::now();
        float writeBackTime = std::chrono::duration<float, std::milli>(checkpoint4 - lastCheckpoint).count();
        lastCheckpoint = checkpoint4;

        // === 3) 构建 WorldContext 并派发事件到实体 ===
        // 准备只读查询接口
        EntityQuery entityQuery{};
        entityQuery.entityMap = &id2ptr_;

        // 构建上下文：提供给所有实体的只读物理查询、实体查询和命令缓冲
        WorldContext ctx{};
        ctx.time = time_;
        ctx.dt = dt;
        ctx.physics = &query_; // 物理查询（包含触发器重叠状态）
        ctx.entities = &entityQuery; // 实体查询
        ctx.commands = &cmdBuffer_; // 命令缓冲（用于生成/销毁实体）
        ctx.resources = getResourceManager(); // 资源管理器（子类提供）
        ctx.currentrenderer = renderer_;
        ctx.camera = getCameraForShake(); // 相机指针（子类提供，用于画面抖动等效果）

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

        auto checkpoint5 = std::chrono::high_resolution_clock::now();
        float collisionEventTime = std::chrono::duration<float, std::milli>(checkpoint5 - lastCheckpoint).count();
        lastCheckpoint = checkpoint5;

        // 3.2) 逐实体更新逻辑
        // 调用每个实体的 update() 方法，实体可以：
        // - 查询物理状态（如触发器重叠）
        // - 修改自身状态
        // - 通过 ctx.commands 发送生成/销毁命令
        for (auto &ptr: entities_) {
            if (ptr) ptr->update(ctx, dt);
        }

        auto checkpoint6 = std::chrono::high_resolution_clock::now();
        float entityUpdateTime = std::chrono::duration<float, std::milli>(checkpoint6 - lastCheckpoint).count();
        lastCheckpoint = checkpoint6;

        // 3.3) 更新UI元素
        for (auto &elem: uiElements_) {
            if (elem) elem->update(dt);
        }

        auto checkpoint7 = std::chrono::high_resolution_clock::now();
        float uiUpdateTime = std::chrono::duration<float, std::milli>(checkpoint7 - lastCheckpoint).count();
        lastCheckpoint = checkpoint7;

        // 4) 提交命令缓冲
        submitCommands();

        auto checkpoint8 = std::chrono::high_resolution_clock::now();
        float submitCommandTime = std::chrono::duration<float, std::milli>(checkpoint8 - lastCheckpoint).count();

        float totalTime = std::chrono::duration<float, std::milli>(checkpoint8 - frameStart).count();

        // 每60帧输出一次性能统计
        static int frameCounter = 0;
        if (++frameCounter >= 60) {
            // 计算渲染平均耗时
            float avgMainRender = renderStats_.frameCount > 0
                                      ? renderStats_.mainrenderTime / renderStats_.frameCount
                                      : 0.0f;
            float avgBillboardRender = renderStats_.frameCount > 0
                                           ? renderStats_.billboardTime / renderStats_.frameCount
                                           : 0.0f;
            float avgTrailRender = renderStats_.frameCount > 0
                                       ? renderStats_.trailTime / renderStats_.frameCount
                                       : 0.0f;
            float avgUIRender = renderStats_.frameCount > 0 ? renderStats_.uiTime / renderStats_.frameCount : 0.0f;
            float avgTotalRender = renderStats_.frameCount > 0
                                       ? renderStats_.totalTime / renderStats_.frameCount
                                       : 0.0f;

            printf("\n========== Performance Statistics (Entity count: %zu) ==========\n", entities_.size());
            printf("--- Logic Update ---\n");
            printf("  Sync Transform:   %.3f ms\n", syncTransformTime);
            printf("  Physics Step:     %.3f ms\n", physicsStepTime);
            printf("  Build Query:      %.3f ms\n", buildQueryTime);
            printf("  Write Back:       %.3f ms\n", writeBackTime);
            printf("  Collision Events: %.3f ms\n", collisionEventTime);
            printf("  Entity Update:    %.3f ms\n", entityUpdateTime);
            printf("  UI Update:        %.3f ms\n", uiUpdateTime);
            printf("  Submit Commands:  %.3f ms\n", submitCommandTime);
            printf("  Total Logic:      %.3f ms\n", totalTime);
            printf("\n--- Rendering (avg over %d frames) ---\n", renderStats_.frameCount);
            printf("  Main Render:   %.3f ms\n", avgMainRender);
            printf("  Billboards:       %.3f ms\n", avgBillboardRender);
            printf("  Trails:           %.3f ms\n", avgTrailRender);
            printf("  UI Render:        %.3f ms\n", avgUIRender);
            printf("  Total Render:     %.3f ms\n", avgTotalRender);
            printf("\n--- Overall ---\n");
            printf("  Total Frame Time: %.3f ms (%.1f FPS)\n", totalTime + avgTotalRender,
                   1000.0f / (totalTime + avgTotalRender));
            printf("===============================================================\n\n");

            frameCounter = 0;
            renderStats_.reset();
        }
    }

    // 渲染场景（分层渲染：不透明物体 → Billboard）
    virtual void render() {
        if (!renderer_) return;

        // 性能计时开始
        auto renderStart = std::chrono::high_resolution_clock::now();
        auto lastCheckpoint = renderStart;

        // 获取当前相机（从 Renderer 或子类提供）
        const Camera *camera = getCameraForRendering();

        renderer_->beginFrame();

        auto buildTransformFromWorld = [](const DirectX::XMMATRIX &world) {
            Transform result{};
            DirectX::XMVECTOR scale{};
            DirectX::XMVECTOR rotation{};
            DirectX::XMVECTOR translation{};
            if (DirectX::XMMatrixDecompose(&scale, &rotation, &translation, world)) {
                DirectX::XMStoreFloat3(&result.scale, scale);
                DirectX::XMStoreFloat4(&result.rotation, rotation);
                DirectX::XMStoreFloat3(&result.position, translation);
            }
            return result;
        };

        if (camera) {
            // 收集所有实体渲染数据，提交渲染队列
            for (auto &ptr: entities_) {
                if (!ptr) continue;

                if (isBillboard(ptr.get())) {
                    updateBillboardOrientation(ptr.get(), camera);
                }

                const Model *model = ptr->model();
                if (!model || model->empty()) continue;

                Transform worldTransform = buildTransformFromWorld(ptr->world());

                for (const auto &drawItem: model->drawItems) {
                    if (drawItem.meshIndex >= model->meshes.size()) continue;
                    const auto &meshGpu = model->meshes[drawItem.meshIndex];
                    const Texture *tex = nullptr;
                    if (meshGpu.materialIndex >= 0 &&
                        static_cast<size_t>(meshGpu.materialIndex) < model->materials.size()) {
                        tex = &model->materials[meshGpu.materialIndex].diffuse;
                    }

                    Material tempMaterial = ptr->material() ? *ptr->material() : Material{};
                    if (tex && tex->isValid()) {
                        tempMaterial.baseColor = tex->srv();
                    }

                    Renderer::DrawItem item{};
                    item.mesh = &meshGpu.mesh;
                    item.material = &tempMaterial;
                    item.transform = worldTransform;
                    item.transparent = ptr->isTransparent();
                    item.alpha = ptr->getAlpha();
                    renderer_->submit(item);
                }
            }

            renderer_->endFrame(*camera);
        }

        auto checkpoint1 = std::chrono::high_resolution_clock::now();
        float mainRenderTime = std::chrono::duration<float, std::milli>(checkpoint1 - lastCheckpoint).count();
        lastCheckpoint = checkpoint1;

        // 调试模式下的碰撞体线框（单独绘制）
        if (camera) {
            for (auto &ptr: entities_) {
                if (!ptr) continue;
                auto span = ptr->colliders();
                std::vector<ColliderBase *> cols(span.begin(), span.end());
                if (!cols.empty()) {
                    for (auto *c: cols) {
                        renderer_->drawColliderWire(*c);
                    }
                }
            }
        }

        // 渲染 Trail（自定义渲染路径）
        renderTrails(camera);

        auto checkpoint2 = std::chrono::high_resolution_clock::now();
        float trailRenderTime = std::chrono::duration<float, std::milli>(checkpoint2 - lastCheckpoint).count();
        lastCheckpoint = checkpoint2;

        // 渲染 UI 元素（按layer排序，始终在最上层）
        renderUI();

        auto checkpoint3 = std::chrono::high_resolution_clock::now();
        float uiRenderTime = std::chrono::duration<float, std::milli>(checkpoint3 - lastCheckpoint).count();

        float totalRenderTime = std::chrono::duration<float, std::milli>(checkpoint3 - renderStart).count();

        renderStats_.mainrenderTime += mainRenderTime;
        renderStats_.billboardTime += 0.0f;
        renderStats_.trailTime += trailRenderTime;
        renderStats_.uiTime += uiRenderTime;
        renderStats_.totalTime += totalRenderTime;
        renderStats_.frameCount++;
    }

    // 判断实体是否是Billboard（通过类型检测）
    virtual bool isBillboard(IEntity *entity) const {
        // 需要前向声明或包含 BillboardEntity.hpp
        // 这里使用简单的名称检测作为临时方案
        return false; // 子类可重写
    }

    // 获取渲染用的相机（子类重写提供具体相机）
    virtual const Camera *getCameraForRendering() const {
        return nullptr; // 默认返回空，BattleScene 等子类会重写
    }

    // 获取用于画面抖动的相机（子类重写提供具体相机）
    virtual Camera *getCameraForShake() {
        return nullptr; // 默认返回空，BattleScene 等子类会重写
    }

    // Billboard 专用渲染方法
    virtual void renderBillboards(const Camera *camera) {
        if (!camera) return;

        // 收集所有 Billboard 实体（通过虚函数判断）
        std::vector<IEntity *> billboards;
        for (auto &ptr: entities_) {
            if (isBillboard(ptr.get())) {
                billboards.push_back(ptr.get());
            }
        }

        if (billboards.empty()) return;

        // 按距离排序（从远到近，避免透明物体之间的遮挡问题）
        DirectX::XMFLOAT3 camPos = camera->getPosition();
        std::sort(billboards.begin(), billboards.end(),
                  [&camPos](IEntity *a, IEntity *b) {
                      DirectX::XMFLOAT3 posA = a->transformRef().position;
                      DirectX::XMFLOAT3 posB = b->transformRef().position;

                      float distASq = (posA.x - camPos.x) * (posA.x - camPos.x) +
                                      (posA.y - camPos.y) * (posA.y - camPos.y) +
                                      (posA.z - camPos.z) * (posA.z - camPos.z);
                      float distBSq = (posB.x - camPos.x) * (posB.x - camPos.x) +
                                      (posB.y - camPos.y) * (posB.y - camPos.y) +
                                      (posB.z - camPos.z) * (posB.z - camPos.z);

                      return distASq > distBSq; // 从远到近
                  });

        // 设置透明渲染状态
        renderer_->setAlphaBlending(true);
        renderer_->setDepthWrite(false); // 不写入深度，但保持深度测试
        renderer_->setBackfaceCulling(false); // 双面渲染

        // 渲染所有 Billboard（需要更新朝向）
        for (auto *entity: billboards) {
            updateBillboardOrientation(entity, camera);
            if (entity->model()) {
                renderer_->draw(*entity);
            }
        }

        // 恢复默认渲染状态
        renderer_->setAlphaBlending(false);
        renderer_->setDepthWrite(true);
        renderer_->setBackfaceCulling(true);
    }

    // 更新 Billboard 朝向（子类可重写以提供具体实现）
    virtual void updateBillboardOrientation(IEntity *entity, const Camera *camera) {
        // 默认实现为空，BattleScene 等子类会重写
    }

    // Trail 专用渲染方法
    virtual void renderTrails(const Camera *camera);

    // 判断实体是否是 Trail（通过类型检测）
    virtual bool isTrail(IEntity *entity) const;

    // 访问底层 PhysicsWorld
    PhysicsWorld &physics() { return world_; }

    // 访问 Renderer（用于实体工厂）
    Renderer *renderer() { return renderer_; }

    // 访问 ResourceManager（子类需要实现，返回其持有的 ResourceManager）
    virtual ResourceManager *getResourceManager() { return nullptr; }

    // 输入处理接口（子类实现具体的交互逻辑）
    virtual void handleInput(float dt, const void *window) {
    }

    // 获取实体映射表（用于 InputManager 射线检测）
    const std::unordered_map<EntityId, IEntity *> *getEntityMap() const { return &id2ptr_; }

    void setSceneManager(SceneManager *manager) { manager_ = manager; }
    SceneManager *sceneManager() const { return manager_; }

    // UI管理接口
    void addUIElement(std::unique_ptr<class UIElement> element);

    void removeUIElement(class UIElement *element);

    void clearUIElements();

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

    SceneManager *manager_ = nullptr;

    // UI元素容器
    std::vector<std::unique_ptr<class UIElement> > uiElements_;
    ResourceManager resourceManager_;

    // 渲染性能统计结构
    struct RenderStats {
        float mainrenderTime = 0.0f;
        float billboardTime = 0.0f;
        float trailTime = 0.0f;
        float uiTime = 0.0f;
        float totalTime = 0.0f;
        int frameCount = 0;

        void reset() {
            mainrenderTime = billboardTime = trailTime = uiTime = totalTime = 0.0f;
            frameCount = 0;
        }
    };

    RenderStats renderStats_;

    // UI专用渲染方法
    virtual void renderUI();

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
};

// Trail 渲染实现（需要前向声明）
#include "game/entity/TrailEntity.hpp"

inline void Scene::renderTrails(const Camera *camera) {
    if (!camera || !renderer_) return;

    // 收集所有 Trail 实体
    std::vector<TrailEntity *> trails;
    for (auto &ptr: entities_) {
        if (auto *trail = dynamic_cast<TrailEntity *>(ptr.get())) {
            trails.push_back(trail);
        }
    }

    if (trails.empty()) return;

    // 设置透明渲染状态（与 Billboard 相同）
    renderer_->setAlphaBlending(true);
    renderer_->setDepthWrite(false); // 不写入深度，但保持深度测试
    renderer_->setBackfaceCulling(false); // 双面渲染

    // 渲染所有 Trail
    for (auto *trail: trails) {
        trail->render(renderer_, time_);
    }

    // 恢复默认渲染状态
    renderer_->setAlphaBlending(false);
    renderer_->setDepthWrite(true);
    renderer_->setBackfaceCulling(true);
}

inline bool Scene::isTrail(IEntity *entity) const {
    return dynamic_cast<TrailEntity *>(entity) != nullptr;
}

// UI渲染实现
#include "game/ui/UIElement.hpp"
#include <algorithm>

inline void Scene::renderUI() {
    if (!renderer_ || uiElements_.empty()) return;

    // 按layer排序（从小到大，后渲染的在前方）
    std::vector<UIElement *> sortedUI;
    for (auto &elem: uiElements_) {
        if (elem) sortedUI.push_back(elem.get());
    }

    std::sort(sortedUI.begin(), sortedUI.end(),
              [](UIElement *a, UIElement *b) {
                  return a->layer() < b->layer();
              });

    // 渲染所有UI元素
    for (auto *elem: sortedUI) {
        elem->render(renderer_);
    }
}

inline void Scene::addUIElement(std::unique_ptr<UIElement> element) {
    if (element) {
        element->init();
        uiElements_.push_back(std::move(element));
    }
}

inline void Scene::removeUIElement(UIElement *element) {
    auto it = std::find_if(uiElements_.begin(), uiElements_.end(),
                           [element](const std::unique_ptr<UIElement> &ptr) {
                               return ptr.get() == element;
                           });
    if (it != uiElements_.end()) {
        uiElements_.erase(it);
    }
}

inline void Scene::clearUIElements() {
    uiElements_.clear();
}

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