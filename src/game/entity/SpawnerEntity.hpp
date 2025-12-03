#pragma once
#include "StaticEntity.hpp"

// 触发型生成器：自身带一个 OBB trigger，未被占用时周期性产出 Ball（通过命令缓冲）
class SpawnerEntity : public StaticEntity {
public:
    float spawnInterval = 0.25f;
    float cooldown = 0.0f;
    float ballRadius = 10.0f;
    float spawnYOffset = -0.6f; // 相对自身原点向下偏移

    void update(WorldContext &ctx, float dt) override {
        cooldown -= dt;
        if (cooldown > 0.0f) return;
        if (!ctx.physics) return;

        // 如果自身触发器未被占用，则生成一个球
        std::vector<EntityId> overlappers;
        ctx.physics->getTriggerOverlappers(id(), overlappers);
        bool blocked = !overlappers.empty();
        if (!blocked && ctx.commands) {
            DirectX::XMFLOAT3 pos = transform.position;
            pos.y += spawnYOffset;
            ctx.commands->spawnBall(pos, ballRadius);
            cooldown = spawnInterval;
        }
    }
};