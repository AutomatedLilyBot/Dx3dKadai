#include "BulletEntity.hpp"
#include "core/gfx/ModelLoader.hpp"
#include "core/resource/ResourceManager.hpp"
#include <DirectXMath.h>

#include "NodeEntity.hpp"
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
    rb.muS = 0.05f;         // 静摩擦：0.6 → 0.05（更容易滚动）
    rb.muK = 0.03f;         // 动摩擦：0.5 → 0.03（滚动时阻力小）
    rb.restitution = 0.7f;  // 弹性系数：0.2 → 0.7（更有弹性）

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

    // 检查速度（如果太慢则删除）
    const auto &v = body->velocity;
    float speedSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (speedSq < minSpeedToLive * minSpeedToLive) {
        printf("[Bullet %llu] Destroying due to low speed (%.2f)\n", id(), sqrtf(speedSq));
        ctx.commands->destroyEntity(id());
        return;
    }

    // 检查生命周期（超时删除）
    if (timeAlive > lifetime) {
        printf("[Bullet %llu] Destroying due to lifetime exceeded (%.2f s)\n", id(), timeAlive);
        ctx.commands->destroyEntity(id());
        return;
    }
}

void BulletEntity::onCollision(WorldContext &ctx, EntityId other, TriggerPhase phase, const OverlapResult &c) {
    if (phase != TriggerPhase::Enter) return;
    auto *otherEntity = ctx.entities->getEntity(other);
    if (!otherEntity) return;

    // 忽略发射者的碰撞（在一段时间内）
    if (other == shooterId && timeAlive < ignoreShooterTime) {
        return;
    }


    if (dynamic_cast<BlockEntity *>(otherEntity)) {
        // Collision with BlockEntity is handled by BlockEntity::onCollision (responseType), so do nothing here.
    }
    else if (dynamic_cast<NodeEntity *>(otherEntity)) {
        NodeEntity* hitnode=dynamic_cast<NodeEntity *>(otherEntity);
        hitnode->onHitByBullet(ctx,this->team);
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
