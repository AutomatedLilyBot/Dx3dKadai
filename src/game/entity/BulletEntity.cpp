#include "BulletEntity.hpp"
#include "core/gfx/ModelLoader.hpp"
#include "core/resource/ResourceManager.hpp"
#include <DirectXMath.h>

#include "NodeEntity.hpp"
#include "ExplosionEffect.hpp"
#include "TrailEntity.hpp"
#include "game/runtime/WorldContext.hpp"

using namespace DirectX;

auto clampDirToXZ(const DirectX::XMFLOAT3 &dir) {
    XMFLOAT3 c = dir;
    c.y = 0.0f;
    return c;
}

// 新版本：使用 ResourceManager（推荐）
void BulletEntity::initialize(float radius, const std::wstring &modelPath, ResourceManager *resMgr) {
    this->setCollider(MakeSphereCollider(radius));
    rb.invMass = 1.0f;

    // 高尔夫球物理参数：低摩擦，高弹性
    rb.muS = 0.02f; // 静摩擦：0.6 → 0.05（更容易滚动）
    rb.muK = 0.01f; // 动摩擦：0.5 → 0.03（滚动时阻力小）
    rb.restitution = 0.8f; // 弹性系数：0.2 → 0.7（更有弹性）

    // 使用 ResourceManager 获取共享模型（自动缓存，无需 modelOwned）
    if (resMgr) {
        modelRef = resMgr->getModel(modelPath);
    } else {
        modelRef = nullptr;
    }
}

// 旧版本：直接加载（已废弃，但保留兼容性）
void BulletEntity::initialize(float radius, const std::wstring &modelPath, ID3D11Device *dev) {
    this->setCollider(MakeSphereCollider(radius));
    rb.invMass = 1.0f;

    // 高尔夫球物理参数：低摩擦，高弹性
    rb.muS = 0.05f;
    rb.muK = 0.03f;
    rb.restitution = 0.7f;

    modelOwned = std::make_unique<Model>();
    ModelLoader::LoadFBX(dev, modelPath, *modelOwned);
    modelRef = modelOwned.get();
}

void BulletEntity::update(WorldContext &ctx, float dt) {
    auto *body = rigidBody();
    if (!body) return;

    // 增加存活时间
    timeAlive += dt;

    switch (this->team) {
        case NodeTeam::Friendly:
            this->materialData.baseColorFactor = XMFLOAT4{0.2f, 0.6f, 1.0f, 0.8f}; // 蓝色
            break;
        case NodeTeam::Enemy:
            this->materialData.baseColorFactor = XMFLOAT4{1.0f, 0.2f, 0.2f, 0.8f}; // 红色
            break;
        case NodeTeam::Neutral:
            this->materialData.baseColorFactor = XMFLOAT4{0.8f, 0.8f, 0.8f, 0.8f}; // 灰色
            break;
    }

    // 更新拖尾效果
    if (enableTrail &&ctx.resources)
    {
        // 创建 Trail（如果还没有）
        if (trailEntityId == 0) {
            ctx.commands->spawn<TrailEntity>([this, &ctx](TrailEntity *trail) {
                trail->transform.position = this->transform.position;
                trail->initialize(ctx.resources);

                // 根据队伍设置不同颜色
                switch (this->team) {
                    case NodeTeam::Friendly:
                        trail->baseColor = XMFLOAT4{0.2f, 0.6f, 1.0f, 0.8f}; // 蓝色
                        break;
                    case NodeTeam::Enemy:
                        trail->baseColor = XMFLOAT4{1.0f, 0.2f, 0.2f, 0.8f}; // 红色
                        break;
                    case NodeTeam::Neutral:
                        trail->baseColor = XMFLOAT4{0.8f, 0.8f, 0.8f, 0.8f}; // 灰色
                        break;
                }

                trail->trailWidth = 0.12f;
                trail->lifetimePerPoint = 0.6f;

                // 记录 Trail ID
                this->trailEntityId = trail->id();
            });
        }

        // 定期向 Trail 添加新点
        trailUpdateTimer += dt;
        if (trailUpdateTimer >= trailUpdateInterval && trailEntityId != 0) {
            trailUpdateTimer = 0.0f;

            // 查找 Trail 实体并添加点
            IEntity *trailEntity = ctx.entities->getEntity(trailEntityId);
            if (auto *trail = dynamic_cast<TrailEntity *>(trailEntity)) {
                trail->addPoint(transform.position, ctx.time);
            }
        }
    }

    // 检查速度（如果太慢则删除）
    const auto &v = body->velocity;
    float speedSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (speedSq < minSpeedToLive * minSpeedToLive) {
        printf("[Bullet %llu] Destroying due to low speed (%.2f)\n", id(), sqrtf(speedSq));
        // 生成爆炸特效
        spawnExplosionEffect(ctx, transform.position);

        // 分离 Trail（让它独立淡出）
        trailEntityId = 0;

        ctx.commands->destroyEntity(id());
        return;
    }

    // 检查生命周期（超时删除）
    if (timeAlive > lifetime) {
        printf("[Bullet %llu] Destroying due to lifetime exceeded (%.2f s)\n", id(), timeAlive);
        // 生成爆炸特效
        spawnExplosionEffect(ctx, transform.position);

        // 分离 Trail（让它独立淡出）
        trailEntityId = 0;

        ctx.commands->destroyEntity(id());
        return;
    }
}

// 辅助方法：在指定位置生成爆炸特效
void BulletEntity::spawnExplosionEffect(WorldContext &ctx, const DirectX::XMFLOAT3 &position) {
    ctx.commands->spawn<ExplosionEffect>([position, &ctx](ExplosionEffect *effect) {
        effect->transform.position = position;
        effect->transform.scale = {1.0f, 1.0f, 1.0f};
        effect->initialize(ctx.resources);
    });
}

void BulletEntity::onCollision(WorldContext &ctx, EntityId other, TriggerPhase phase, const OverlapResult &c) {
    if (phase == TriggerPhase::Exit) return;
    auto *otherEntity = ctx.entities->getEntity(other);
    if (!otherEntity) return;

    // 忽略发射者的碰撞（在一段时间内）
    if (other == shooterId && timeAlive < ignoreShooterTime) {
        return;
    }


    if (dynamic_cast<BlockEntity *>(otherEntity)) {
        // Collision with BlockEntity is handled by BlockEntity::onCollision (responseType), so do nothing here.
    } else if (dynamic_cast<NodeEntity *>(otherEntity)) {
        NodeEntity *hitnode = dynamic_cast<NodeEntity *>(otherEntity);
        hitnode->onHitByBullet(ctx, this->team, this->power);

        // 在碰撞点生成爆炸特效
        XMFLOAT3 collisionPoint;
        XMStoreFloat3(&collisionPoint,
                      XMVectorScale(
                          XMVectorAdd(XMLoadFloat3(&c.pointOnA), XMLoadFloat3(&c.pointOnB)),
                          0.5f
                      )
        );
        spawnExplosionEffect(ctx, collisionPoint);

        // 分离 Trail（让它独立淡出）
        trailEntityId = 0;

        ctx.commands->destroyEntity(id());
    }
}

void BulletEntity::bounceFromSurface(const DirectX::XMFLOAT3 &normal) {
    auto *body = rigidBody();
    if (!body) return;
    XMVECTOR v = XMLoadFloat3(&body->velocity);
    XMVECTOR n = XMLoadFloat3(&normal);
    XMVECTOR r = XMVector3Reflect(v, n);
    XMStoreFloat3(&body->velocity, r);
}