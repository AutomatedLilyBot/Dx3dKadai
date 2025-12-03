#include "Scene.hpp"
#include "game/entity/StaticEntity.hpp"
#include "game/entity/DynamicEntity.hpp"
#include "game/entity/SpawnerEntity.hpp"
#include "game/entity/BallEntity.hpp"
#include "core/gfx/ModelLoader.hpp"
#include <windows.h>
#include <string>

using namespace DirectX;

static std::wstring ExeDirScene() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring s(buf);
    auto p = s.find_last_of(L"\\/");
    return (p == std::wstring::npos) ? L"." : s.substr(0, p);
}

void Scene::init(Renderer *renderer) {
    renderer_ = renderer;
    // 物理参数（占位初始值）
    WorldParams params;
    params.gravity = XMFLOAT3{0, -1.0f, 0};
    world_.setParams(params);

    // 安装物理回调：现在所有碰撞都会产生事件（包括非 trigger），但 trigger 仍不参与解算
    world_.setTriggerCallback([this](EntityId a, EntityId b, TriggerPhase phase, const OverlapResult &contact) {
        // 1) 缓存碰撞事件（双向）：供本帧 later 调用各实体的 onCollision
        tempCollisionEvents_.push_back(CollisionEvent{a, b, phase, contact});
        tempCollisionEvents_.push_back(CollisionEvent{b, a, phase, contact});

        // 2) 仅当对中至少一方是 trigger 碰撞体时，才维护“触发器重叠”视图（供 Spawner 等逻辑查询）
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

    // Demo：构建一个平台（静态立方体）和一个触发型生成器
    // 平台：用 cube.fbx 绘制；碰撞体用 OBB
    {
        auto plat = std::make_unique<StaticEntity>();
        plat->setId(allocId());
        // 视觉：加载模型（临时低效）
        static Model platformModel; // 局部静态，简单起见保证生命周期
        static bool loaded = false;
        if (!loaded) {
            std::wstring cubePath = ExeDirScene() + L"\\asset\\cube.fbx";
            loaded = ModelLoader::LoadFBX(renderer_->device(), cubePath, platformModel);
        }
        if (loaded) plat->modelRef = &platformModel;
        plat->transform.scale = {10.0f, 10.0f, 1.0f};
        plat->transform.position = {0.0f, -2.0f, 0.0f};
        plat->transform.rotationEuler = {XM_PIDIV2, 0, XM_PIDIV2*0.2f};
        // 物理：OBB 半尺寸与缩放相匹配（简化处理）
        plat->collider = MakeObbCollider(XMFLOAT3{10.0f, 10.0, 1.0f});
        plat->collider->setDebugEnabled(true);
        plat->collider->setDebugColor(XMFLOAT4(1, 0, 0, 1));
        // 注册
        registerEntity(*plat);
        id2ptr_[plat->id()] = plat.get();
        entities_.push_back(std::move(plat));
    }

    // 生成器：触发器 OBB，无模型，逻辑在 update 中按占用判定生成球
    {
        auto sp = std::make_unique<SpawnerEntity>();
        sp->setId(allocId());
        sp->modelRef = nullptr; // 不渲染
        sp->transform.position = {0.0f, 3.0f, 0.0f};
        sp->transform.scale = {1.0f, 1.0f, 1.0f};
        sp->collider = MakeObbCollider(XMFLOAT3{0.5f, 0.5f, 0.5f}); // 增大到 0.5（覆盖球体生成区域）
        sp->collider->setIsTrigger(true);
        sp->spawnInterval = 0.15f;
        sp->ballRadius = 0.25f;
        sp->spawnYOffset = -0.5f; // 调整偏移，让球生成在 trigger 内部
        registerEntity(*sp);
        id2ptr_[sp->id()] = sp.get();
        entities_.push_back(std::move(sp));
    }
}

void Scene::buildPhysicsQueryFromLastFrame() {
    // 将 tempTriggerOverlaps_ 固化为本帧只读查询
    triggerOverlaps_ = tempTriggerOverlaps_;
    tempTriggerOverlaps_.clear();
    query_.triggerOverlaps = &triggerOverlaps_;
    // 固化碰撞事件列表
    frameCollisionEvents_ = std::move(tempCollisionEvents_);
    tempCollisionEvents_.clear();
}

void Scene::tick(float dt) {
    time_ += dt;

    // 0.5) 物理前：将外部直接改动的 Transform 写入 PhysicsWorld（若发生变化则清零速度）
    // 说明：仅对静态实体或无刚体的实体同步 Transform，动态体的位置由物理系统控制。
    for (auto &ptr: entities_) {
        if (!ptr) continue;

        const auto &tr = ptr->transformRef();
        world_.syncOwnerTransform(ptr->id(), tr.position, tr.rotationEuler, /*resetVelocityOnChange=*/false);
    }

    // 1) 物理步
    world_.step(dt);

    // 2) 构建（只读）物理查询视图
    buildPhysicsQueryFromLastFrame();

    // 2.5) 将物理解算后的刚体位置写回实体 Transform，确保渲染可见到重力造成的位移
    for (auto &ptr: entities_) {
        if (!ptr) continue;
        if (RigidBody *rb = ptr->rigidBody()) {
            // 动态体：使用 rb.position 作为可视 Transform 的位置
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
    ctx.entities = &entityQuery; // 添加实体查询
    ctx.commands = &cmdBuffer_;

    // 3.1) 派发 onCollision
    for (const auto &ev: frameCollisionEvents_) {
        auto it = id2ptr_.find(ev.a);
        if (it != id2ptr_.end() && it->second) {
            it->second->onCollision(ctx, ev.b, ev.phase, ev.contact);
        }
    }
    frameCollisionEvents_.clear();
    // 3.2) 逐实体 update（Spawner 的逻辑会使用 ctx.commands）
    for (auto &ptr: entities_) {
        if (ptr) ptr->update(ctx, dt);
    }

    // 4) 提交命令缓冲
    submitCommands();
}

void Scene::render() {
    if (!renderer_) return;
    // 绘制所有带模型的实体
    for (auto &ptr: entities_) {
        if (!ptr) continue;
        if (ptr->model()) {
            renderer_->draw(*ptr);
        }
        auto span = ptr->colliders();
        std::vector<ColliderBase *> cols(span.begin(), span.end());
        // 在注册前，将 collider 的位置与实体 Transform 对齐，确保静态体初始世界位姿正确
        //（PhysicsWorld 注册静态体时会用 collider 的当前位置初始化内部 BodyState）
        if (!cols.empty()) {
            for (auto *c: cols) {
                renderer_->drawColliderWire(*c);
            }
        }
    }
}

void Scene::registerEntity(IEntity &e) {
    // 收集 Collider 裸指针
    auto span = e.colliders();
    std::vector<ColliderBase *> cols(span.begin(), span.end());
    // 在注册前，将 collider 的位置与实体 Transform 对齐，确保静态体初始世界位姿正确
    //（PhysicsWorld 注册静态体时会用 collider 的当前位置初始化内部 BodyState）
    if (!cols.empty()) {
        const XMFLOAT3 pos = e.transformRef().position;
        const XMFLOAT3 rot = e.transformRef().rotationEuler;
        for (auto *c: cols) {
            if (!c) continue;
            // 注入 Owner 世界位姿（collider 的世界由此与其局部偏移共同决定）
            c->setOwnerWorldPosition(pos);
            c->setOwnerWorldRotationEuler(rot);
            c->updateDerived();
        }
    }
    RigidBody *rb = e.rigidBody();
    world_.registerEntity(e.id(), rb, std::span<ColliderBase *>(cols.data(), cols.size()));
}

void Scene::unregisterEntity(EntityId id) {
    world_.unregisterEntity(id);
}

void Scene::submitCommands() {
    // Reset 优先
    if (cmdBuffer_.reset.doReset) {
        // 反注册并移除所有动态球（简单起见，移除所有具有刚体的实体，保留第一个平台与生成器）
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
            // 1. 调用 onDestroy() 生命周期钩子（实体仍在场景中，可访问其他实体）
            WorldContext destroyCtx{};
            destroyCtx.time = time_;
            destroyCtx.dt = 0.0f;
            EntityQuery entityQueryForDestroy{};
            entityQueryForDestroy.entityMap = &id2ptr_;
            destroyCtx.physics = &query_;
            destroyCtx.entities = &entityQueryForDestroy;
            destroyCtx.commands = &cmdBuffer_;
            it->second->onDestroy(destroyCtx);

            // 2. 反注册物理
            unregisterEntity(dc.id);

            // 3. 从容器移除
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
        // 1. 调用工厂函数创建实体
        auto entity = cmd.factory(this);
        if (!entity) continue;

        // 2. 分配 ID
        entity->setId(allocId());

        // 3. 调用配置回调（用户自定义配置）
        if (cmd.configurator) {
            cmd.configurator(entity.get());
        }

        // 4. 注册到物理和场景
        registerEntity(*entity);
        EntityId eid = entity->id();
        IEntity *entityPtr = entity.get(); // 保存指针（move前）
        id2ptr_[eid] = entityPtr;
        entities_.push_back(std::move(entity));

        // 5. 调用 init() 生命周期钩子（实体已完全注册）
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

// CommandBuffer::spawnBall 的实现（便捷方法）
void CommandBuffer::spawnBall(const DirectX::XMFLOAT3 &pos, float radius) {
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
        ball->collider->setDebugEnabled(true);
        ball->collider->setDebugColor(DirectX::XMFLOAT4(0, 1, 0, 1));
    };

    spawnEntities.push_back(std::move(cmd));
}
