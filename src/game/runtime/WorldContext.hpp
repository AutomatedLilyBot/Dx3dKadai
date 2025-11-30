#pragma once
#pragma execution_character_set("utf-8")

#include <vector>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <DirectXMath.h>

#include "../src/core/physics/PhysicsWorld.hpp"

// 只读物理查询视图（最小占位实现）：
// 实际项目中应由 Scene 在每帧物理步后填充缓存数据。
struct PhysicsQuery {
    // 指向“本帧触发器重叠映射”：triggerEntity -> { other entity ids }
    const std::unordered_map<EntityId, std::unordered_set<EntityId> > *triggerOverlaps = nullptr;

    bool isAnyTriggerOverlapping(EntityId e) const {
        if (!triggerOverlaps) return false;
        auto it = triggerOverlaps->find(e);
        return it != triggerOverlaps->end() && !it->second.empty();
    }

    bool isTriggerOverlapping(EntityId triggerEntity, EntityId withEntity) const {
        if (!triggerOverlaps) return false;
        auto it = triggerOverlaps->find(triggerEntity);
        if (it == triggerOverlaps->end()) return false;
        return it->second.find(withEntity) != it->second.end();
    }

    size_t getTriggerOverlappers(EntityId triggerEntity, std::vector<EntityId> &out) const {
        out.clear();
        if (!triggerOverlaps) return 0;
        auto it = triggerOverlaps->find(triggerEntity);
        if (it == triggerOverlaps->end()) return 0;
        out.reserve(it->second.size());
        for (auto id: it->second) out.push_back(id);
        return out.size();
    }
};

// 命令缓冲区（最小占位实现）：延后到提交阶段统一执行。
struct CommandBuffer {
    struct SpawnBallCmd {
        DirectX::XMFLOAT3 pos;
        float radius;
    };

    struct DestroyCmd {
        EntityId id;
    };

    struct ResetCmd {
        bool doReset = false;
    };

    std::vector<SpawnBallCmd> spawnBalls;
    std::vector<DestroyCmd> toDestroy;
    ResetCmd reset;

    void spawnBall(const DirectX::XMFLOAT3 &pos, float radius) { spawnBalls.push_back({pos, radius}); }
    void destroyEntity(EntityId e) { toDestroy.push_back({e}); }
    void resetScene() { reset.doReset = true; }

    void clear() {
        spawnBalls.clear();
        toDestroy.clear();
        reset.doReset = false;
    }
};

// 每帧注入到实体 update/onTrigger 的上下文
struct WorldContext {
    float time = 0.0f;
    float dt = 0.0f;
    const PhysicsQuery *physics = nullptr; // 只读
    CommandBuffer *commands = nullptr; // 可写
};
