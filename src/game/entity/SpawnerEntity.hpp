#pragma once
#include <random>

#include "StaticEntity.hpp"
#include "../runtime/WorldContext.hpp"

// 触发型生成器：自身带一个 OBB trigger，未被占用时周期性产出 Ball（通过命令缓冲）
class SpawnerEntity : public StaticEntity {
public:
    float spawnInterval = 1.0f;
    float cooldown = 0.0f;
    float ballRadius = 10.0f;
    float spawnYOffset = -0.1f; // 相对自身原点向下偏移

    void update(WorldContext &ctx, float dt) override {
        cooldown -= dt;
        if (cooldown > 0.0f) return;
        if (!ctx.physics) return;

        // 如果自身触发器未被占用，则生成一个球
        std::vector<EntityId> overlappers;
        ctx.physics->getTriggerOverlappers(id(), overlappers);
        bool blocked = !overlappers.empty();
        //bool blocked=false;
        if (!blocked && ctx.commands) {
            DirectX::XMFLOAT3 pos = transform.position;
            pos.y += spawnYOffset;

            // std::mt19937 rng(1337);
            // std::uniform_int_distribution<int> distXZ(-2, 2);
            // std::uniform_real_distribution<float> jitter(-2, 2);
            //
            // pos.x+=jitter(rng);
            // pos.z+=jitter(rng);

            ctx.commands->spawnBall(pos, ballRadius);
            wprintf(L"spawncmd sent\n");
            cooldown = spawnInterval;
        }
    }
};