#include "NodeEntity.hpp"
#include <DirectXMath.h>

using namespace DirectX;

void NodeEntity::update(WorldContext &ctx, float dt) {
    if (state == NodeState::Firing) {
        fireTimer += dt;
        if (fireTimer >= fireInterval) {
            fireBullet(ctx);
            fireTimer = 0.0f;
        }
    }
}

void NodeEntity::setFacingDirection(const DirectX::XMFLOAT3 &worldTarget) {
    XMFLOAT3 dir{
        worldTarget.x - transform.position.x,
        0.0f,
        worldTarget.z - transform.position.z
    };
    XMVECTOR v = XMLoadFloat3(&dir);
    v = XMVector3Normalize(v);
    XMStoreFloat3(&facingDirection, v);
    float yaw = std::atan2(facingDirection.x, facingDirection.z);
    transform.setRotationEuler(0.0f, yaw, 0.0f);
}

bool NodeEntity::isFrontClear(WorldContext &ctx) const {
    if (!frontTrigger) return true;
    return !ctx.physics->isAnyTriggerOverlapping(id());
}

void NodeEntity::fireBullet(WorldContext &ctx) {
    if (!ctx.commands) return;
    ctx.commands->spawn<BulletEntity>([&](BulletEntity *b) {
        b->team = team;
        b->transform.position = transform.position;
        b->initialize(bulletRadius, L"asset/ball.fbx", nullptr);
        b->rb.invMass = 1.0f;
        b->rb.velocity = {facingDirection.x * bulletSpeed, facingDirection.y * bulletSpeed, facingDirection.z * bulletSpeed};
    });
}

void NodeEntity::startFiring() {
    state = NodeState::Firing;
    fireTimer = 0.0f;
}

void NodeEntity::stopFiring() {
    state = NodeState::Idle;
}

void NodeEntity::onHitByBullet(WorldContext &ctx, NodeTeam attackerTeam) {
    if (team == attackerTeam) return;
    team = attackerTeam;
}
