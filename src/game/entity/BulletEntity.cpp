#include "BulletEntity.hpp"
#include "core/gfx/ModelLoader.hpp"
#include <DirectXMath.h>

using namespace DirectX;

auto clampDirToXZ(const DirectX::XMFLOAT3 &dir) {
    XMFLOAT3 c = dir;
    c.y = 0.0f;
    return c;
}

void BulletEntity::initialize(float radius, const std::wstring &modelPath, ID3D11Device *dev) {
    collider_ = MakeSphereCollider(radius);
    rb.invMass = 1.0f;
    modelOwned = std::make_unique<Model>();
    ModelLoader::LoadFBX(dev, modelPath, *modelOwned);
    modelRef = modelOwned.get();
}

void BulletEntity::update(WorldContext &ctx, float dt) {
    auto *body = rigidBody();
    if (!body) return;
    const auto &v = body->velocity;
    float speedSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (speedSq < minSpeedToLive * minSpeedToLive) {
        ctx.commands->destroyEntity(id());
    }
}

void BulletEntity::onCollision(WorldContext &ctx, EntityId other, TriggerPhase phase, const OverlapResult &c) {
    if (phase != TriggerPhase::Enter) return;
    auto *otherEntity = ctx.entities->getEntity(other);
    if (!otherEntity) return;
    if (dynamic_cast<BlockEntity *>(otherEntity)) {
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
