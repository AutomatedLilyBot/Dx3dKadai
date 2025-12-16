#pragma once
#include "StaticEntity.hpp"
#include "BulletEntity.hpp"
#include <memory>

enum class NodeState {
    Idle,
    Firing
};

class NodeEntity : public StaticEntity {
public:
    NodeTeam team = NodeTeam::Neutral;
    NodeState state = NodeState::Idle;

    float fireInterval = 2.0f;
    float fireTimer = 0.0f;
    float bulletSpeed = 10.0f;
    float bulletRadius = 0.25f;

    std::unique_ptr<CapsuleCollider> bodyCollider;
    std::unique_ptr<OBBCollider> frontTrigger;
    DirectX::XMFLOAT3 facingDirection{0, 0, 1};

    void update(WorldContext &ctx, float dt) override;
    void setFacingDirection(const DirectX::XMFLOAT3 &worldTarget);
    bool isFrontClear(WorldContext &ctx) const;
    void fireBullet(WorldContext &ctx);
    void startFiring();
    void stopFiring();
    void onHitByBullet(WorldContext &ctx, NodeTeam attackerTeam);
};
