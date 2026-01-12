#include "NodeEntity.hpp"
#include <DirectXMath.h>
#include "game/runtime/WorldContext.hpp"

using namespace DirectX;

// Getter 和 Setter 实现
float NodeEntity::getfireinterval() const {
    return fireInterval;
}

void NodeEntity::setfireinterval(float interval) {
    fireInterval = interval;
}

float NodeEntity::getbulletspeed() const {
    return bulletSpeed;
}

void NodeEntity::setbulletspeed(float speed) {
    bulletSpeed = speed;
}

float NodeEntity::getbulletradius() const {
    return bulletRadius;
}

void NodeEntity::setbulletradius(float radius) {
    bulletRadius = radius;
}

NodeTeam NodeEntity::getteam() const {
    return team;
}

void NodeEntity::setteam(NodeTeam team) {
    this->team = team;
}

NodeState NodeEntity::getstate() const {
    return state;
}

void NodeEntity::setstate(NodeState state) {
    this->state = state;
}

DirectX::XMFLOAT3 NodeEntity::getFacingDirection() const {
    return facingDirection;
}


void NodeEntity::resetfiretimer() {
    fireTimer = 0.0f;
}

void NodeEntity::update(WorldContext &ctx, float dt) {
    if (state == NodeState::Firing) {
        fireTimer += dt;
        if (fireTimer >= fireInterval) {
            fireBullet(ctx);
            fireTimer = 0.0f;
        }
    }
    switch (this->team) {
        case NodeTeam::Friendly:
            this->materialData.baseColorFactor=XMFLOAT4(0,0,1,1);
            break;
        case NodeTeam::Neutral:
            this->materialData.baseColorFactor=XMFLOAT4(1,1,1,1);
            break;
        case NodeTeam::Enemy:
            this->materialData.baseColorFactor=XMFLOAT4(1,0,0,1);
            break;
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
    ColliderBase* trigger=colliders_.at(1).get();
    if (!trigger||!trigger->isTrigger()) return false;

    return !ctx.physics->isAnyTriggerOverlapping(id());
}

void NodeEntity::fireBullet(WorldContext &ctx) {
    if (!ctx.commands) return;
    ctx.commands->spawn<BulletEntity>([&](BulletEntity *b) {
        b->team = team;
        b->shooterId = this->id();  // 设置发射者 ID，避免刚生成就碰撞
        b->transform.position = this->colliders_.at(0)->getWorldPosition();
        b->power=firepower;
        // 使用 ResourceManager 初始化（从 WorldContext 获取）
        b->initialize(bulletRadius, L"asset/ball.fbx", ctx.resources);
        b->rb.invMass = 1.0f;
        b->rb.velocity = {
            facingDirection.x * bulletSpeed,
            facingDirection.y * bulletSpeed,
            facingDirection.z * bulletSpeed
        };
    });
}

void NodeEntity::startFiring() {
    state = NodeState::Firing;
    //fireTimer = 0.0f;
}

void NodeEntity::stopFiring() {
    state = NodeState::Idle;
    fireTimer = 0.0f;
}

void NodeEntity::onHitByBullet(WorldContext &ctx, NodeTeam attackerTeam, int power) {
    if (team == attackerTeam) {
        health = min(health+2*power, 30);
        fireInterval=2.0/(1+health*0.1);
        firepower=static_cast<int>(1 + floor(health * 0.1));
    }
    else {
        health -= power;
        if (health<=0) {
            team=attackerTeam;
            stopFiring();
            health= 1;
        }
    }
}

int NodeEntity::gethealth() const {
    return health;
}

int NodeEntity::getfirepower() const {
    return firepower;
}

