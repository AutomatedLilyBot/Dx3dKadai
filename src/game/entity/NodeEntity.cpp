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
            this->materialData.baseColorFactor = XMFLOAT4(0, 0, 1, 1);
            break;
        case NodeTeam::Neutral:
            this->materialData.baseColorFactor = XMFLOAT4(1, 1, 1, 1);
            break;
        case NodeTeam::Enemy:
            this->materialData.baseColorFactor = XMFLOAT4(1, 0, 0, 1);
            break;
    }

    // 演示模式下所有节点都启用AI
    if (isDemoMode_) {
        updateAI(ctx, dt);
    }
    // Enemy AI
    else if (team == NodeTeam::Enemy) {
        updateAI(ctx, dt);
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
    ColliderBase *trigger = colliders_.at(1).get();
    if (!trigger || !trigger->isTrigger()) return false;

    return !ctx.physics->isAnyTriggerOverlapping(id());
}

void NodeEntity::fireBullet(WorldContext &ctx) {
    if (!ctx.commands) return;
    ctx.commands->spawn<BulletEntity>([&](BulletEntity *b) {
        b->team = team;
        b->shooterId = this->id(); // 设置发射者 ID，避免刚生成就碰撞
        b->transform.position = this->colliders_.at(0)->getWorldPosition();
        b->power = firePower;
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
    // 演示模式下忽略血量和队伍操作
    if (isDemoMode_) {
        return;
    }

    if (team == attackerTeam) {
        health = min(health + 3 * power, static_cast<int>(maxHealth));
        fireInterval = 2.0 / (1 + health * 0.1);
        firePower = static_cast<int>(1 + floor(health * 0.1));
    } else {
        health -= power;
        if (health <= 0) {
            // 记录旧队伍用于判断是否触发抖动
            NodeTeam oldTeam = team;

            team = attackerTeam;
            stopFiring();
            health = 1;

            // 触发画面抖动的条件判断
            // 1. 玩家的子弹导致中立/敌人节点变成玩家队伍
            // 2. 敌人的子弹导致玩家节点变成敌人队伍
            bool shouldShake = false;
            if (attackerTeam == NodeTeam::Friendly && (oldTeam == NodeTeam::Neutral || oldTeam == NodeTeam::Enemy)) {
                // 玩家占领了中立或敌人节点
                shouldShake = true;
            } else if (attackerTeam == NodeTeam::Enemy && oldTeam == NodeTeam::Friendly) {
                // 敌人占领了玩家节点
                shouldShake = true;
            }

            // 触发抖动
            if (shouldShake && ctx.camera) {
                ctx.camera->triggerShake(0.3f, 0.3f);
            }
        }
    }
}

int NodeEntity::getHealth() const {
    return health;
}

int NodeEntity::getFirepower() const {
    return firePower;
}

float NodeEntity::getHealthRatio() const {
    if (maxHealth <= 0.0f) return 0.0f;
    return std::clamp(static_cast<float>(health) / maxHealth, 0.0f, 1.0f);
}

float NodeEntity::getAlpha() const {
    return std::clamp(getHealthRatio(), 0.4f, 0.9f);
}

EntityId NodeEntity::findNearestEnemyNode(WorldContext &ctx) const {
    if (!ctx.entities) return 0;

    EntityId nearestId = 0;
    float minDistanceSq = FLT_MAX;

    std::vector<EntityId> allIds;
    ctx.entities->getAllEntityIds(allIds);

    for (EntityId id: allIds) {
        if (id == this->id()) continue; // 跳过自己

        IEntity *entity = ctx.entities->getEntity(id);
        if (!entity) continue;

        // 尝试转换为 NodeEntity
        NodeEntity *node = dynamic_cast<NodeEntity *>(entity);
        if (!node) continue;

        // 检查是否是敌对队伍
        if (node->getteam() == this->team) continue;

        // 计算距离平方（避免开方运算）
        XMFLOAT3 diff{
            node->transform.position.x - this->transform.position.x,
            0.0f,
            node->transform.position.z - this->transform.position.z
        };
        float distSq = diff.x * diff.x + diff.z * diff.z;

        if (distSq < minDistanceSq) {
            minDistanceSq = distSq;
            nearestId = id;
        }
    }

    return nearestId;
}

void NodeEntity::updateAI(WorldContext &ctx, float dt) {
    aiUpdateTimer += dt;

    // 每隔固定时间更新一次决策
    if (aiUpdateTimer >= aiUpdateInterval) {
        aiUpdateTimer = 0.0f;

        // 寻找最近的敌方节点
        EntityId nearestEnemy = findNearestEnemyNode(ctx);

        if (nearestEnemy != 0) {
            aiTargetId = nearestEnemy;

            // 获取目标实体
            IEntity *targetEntity = ctx.entities->getEntity(aiTargetId);
            if (targetEntity) {
                // 转向目标
                setFacingDirection(targetEntity->transformRef().position);
                // 开始射击
                startFiring();
            }
        } else {
            // 没有目标，停止射击
            stopFiring();
            aiTargetId = 0;
        }
    }
}

void NodeEntity::setDemoMode(bool enabled) {
    isDemoMode_ = enabled;
    if (enabled) {
        // 设置演示模式的固定参数
        fireInterval = DEMO_FIRE_INTERVAL;
        firePower = DEMO_FIRE_POWER;
    }
}