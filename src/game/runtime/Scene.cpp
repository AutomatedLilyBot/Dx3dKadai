#include "Scene.hpp"
#include "../entity/StaticEntity.hpp"
#include "../entity/DynamicEntity.hpp"
#include "../entity/SpawnerEntity.hpp"
#include "../src/core/gfx/ModelLoader.hpp"
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
    params.gravity = XMFLOAT3{0, -9.8f, 0};
    params.substeps = 1;
    world_.setParams(params);

    // 安装触发器回调（当前仅占位，不做路由）
    world_.setTriggerCallback([this](EntityId a, EntityId b, TriggerPhase phase, const OverlapResult &contact) {
        // 将回调路由到帧缓存：仅记录 Trigger 对（实体维度）
        (void)contact;
        auto addPair = [&](EntityId ta, EntityId tb) {
            tempTriggerOverlaps_[ta].insert(tb);
        };
        auto removePair = [&](EntityId ta, EntityId tb) {
            auto it = triggerOverlaps_.find(ta);
            if (it != triggerOverlaps_.end()) it->second.erase(tb);
            auto it2 = tempTriggerOverlaps_.find(ta);
            if (it2 != tempTriggerOverlaps_.end()) it2->second.erase(tb);
        };
        // 这里不强制检查 collider 是否为 trigger，直接由上层按“谁是 trigger 实体”来查询
        if (phase == TriggerPhase::Exit) {
            removePair(a, b);
            removePair(b, a);
        } else {
            addPair(a, b);
            addPair(b, a);
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
        plat->transform.scale = {100.0f, 100.0f, 100.0f};
        plat->transform.position = {0.0f, 0.0f, 0.0f};
        plat->transform.rotationEuler = {XM_PIDIV2, 0, 0};
        // 物理：OBB 半尺寸与缩放相匹配（简化处理）
        plat->collider = MakeObbCollider(XMFLOAT3{5.0f, 0.5f, 5.0f});
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
        sp->collider = MakeObbCollider(XMFLOAT3{0.25f, 0.25f, 0.25f});
        sp->collider->setIsTrigger(true);
        sp->spawnInterval = 0.15f;
        sp->ballRadius = 0.25f;
        sp->spawnYOffset = -0.6f;
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
}

void Scene::tick(float dt) {
    time_ += dt;

    // 1) 物理步
    world_.step(dt);

    // 2) 构建（只读）物理查询视图
    buildPhysicsQueryFromLastFrame();

    // 3) 遍历实体并 update（Spawner 的逻辑会使用 ctx.commands）
    WorldContext ctx{};
    ctx.time = time_;
    ctx.dt = dt;
    ctx.physics = &query_;
    ctx.commands = &cmdBuffer_;
    for (auto &ptr : entities_) {
        if (ptr) ptr->update(ctx, dt);
    }

    // 4) 提交命令缓冲
    submitCommands();
}

void Scene::render() {
    if (!renderer_) return;
    // 绘制所有带模型的实体
    for (auto &ptr : entities_) {
        if (!ptr) continue;
        if (ptr->model()) {
            renderer_->draw(*ptr);
        }
    }
}

void Scene::registerEntity(IEntity &e) {
    // 收集 Collider 裸指针
    auto span = e.colliders();
    std::vector<ColliderBase *> cols(span.begin(), span.end());
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
        for (size_t i = 0; i < entities_.size(); ) {
            auto *e = entities_[i].get();
            if (e && e->rigidBody()!=nullptr) {
                unregisterEntity(e->id());
                id2ptr_.erase(e->id());
                entities_.erase(entities_.begin()+i);
                continue;
            }
            ++i;
        }
        cmdBuffer_.clear();
        return;
    }

    // 销毁命令
    for (auto &dc : cmdBuffer_.toDestroy) {
        auto it = id2ptr_.find(dc.id);
        if (it != id2ptr_.end()) {
            unregisterEntity(dc.id);
            // 从容器移除
            for (size_t i=0;i<entities_.size();++i){
                if (entities_[i].get()==it->second){ entities_.erase(entities_.begin()+i); break; }
            }
            id2ptr_.erase(it);
        }
    }

    // 生成球命令
    for (auto &sc : cmdBuffer_.spawnBalls) {
        // 创建一个动态实体，加载 cube 模型（临时代用），设置球形碰撞体
        struct BallEntity final : public DynamicEntity {
            BallEntity(ID3D11Device* dev, const std::wstring &path, float radius) {
                // 模型加载（低效但按需求）
                modelOwned = std::make_unique<Model>();
                ModelLoader::LoadFBX(dev, path, *modelOwned);
                modelRef = modelOwned.get();
                collider = MakeSphereCollider(radius);
                rb.invMass = 1.0f; // 质量=1
            }
            std::unique_ptr<Model> modelOwned;
        };
        auto ball = std::make_unique<BallEntity>(renderer_->device(), ExeDirScene()+L"\\asset\\ball.fbx", sc.radius);
        ball->setId(allocId());
        ball->rb.position = sc.pos;
        ball->transform.position = sc.pos;
        // 注册
        registerEntity(*ball);
        id2ptr_[ball->id()] = ball.get();
        entities_.push_back(std::move(ball));
    }

    cmdBuffer_.clear();
}
