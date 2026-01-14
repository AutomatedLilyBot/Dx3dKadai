#pragma once
#include "StaticEntity.hpp"
#include "BulletEntity.hpp"
#include <memory>
#include <algorithm>

enum class NodeState {
    Idle,
    Firing
};

class NodeEntity : public StaticEntity {
public:
    float getfireinterval() const;
    void setfireinterval(float interval);
    float getbulletspeed() const;
    void setbulletspeed(float speed);
    float getbulletradius() const;
    void setbulletradius(float radius);
    NodeTeam getteam() const;
    void setteam(NodeTeam team);
    NodeState getstate() const;
    void setstate(NodeState state);
    DirectX::XMFLOAT3 getFacingDirection() const;
    void setfacingdirection(const DirectX::XMFLOAT3 &direction);
    int getHealth() const;
    int getFirepower() const;
    float getHealthRatio() const;

    bool isTransparent() const override { return true; }

    float getAlpha() const override;

    void update(WorldContext &ctx, float dt) override;
    void setFacingDirection(const DirectX::XMFLOAT3 &worldTarget);
    bool isFrontClear(WorldContext &ctx) const;
    void fireBullet(WorldContext &ctx);
    void startFiring();
    void stopFiring();
    void resetfiretimer();
    void onHitByBullet(WorldContext &ctx, NodeTeam attackerTeam, int power);

    // 判断是否需要显示指示箭头（友方且正在开火）
    bool shouldShowDirectionIndicator() const {
        return team == NodeTeam::Friendly && state == NodeState::Firing;
    }

private:
    // AI 相关
    void updateAI(WorldContext &ctx, float dt);

    EntityId findNearestEnemyNode(WorldContext &ctx) const;

private:
    float fireInterval = 2.0f;
    float fireTimer = 0.0f;
    int firePower = 2;
    float bulletSpeed = 10.0f;
    float bulletRadius = 0.25f;
    int health = 10;
    float maxHealth = 30.0f;

public:


private:
    NodeTeam team = NodeTeam::Neutral;
    NodeState state = NodeState::Idle;
    DirectX::XMFLOAT3 facingDirection{0, 0, 1};

    // AI 状态
    float aiUpdateInterval = 0.5f; // AI 决策更新间隔
    float aiUpdateTimer = 0.0f;
    EntityId aiTargetId = 0; // 当前 AI 锁定的目标
};
