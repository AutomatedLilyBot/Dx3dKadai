#pragma once
#pragma execution_character_set("utf-8")

#include <vector>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>
#include <DirectXMath.h>

#include "../src/core/physics/PhysicsWorld.hpp"

// 前置声明
struct IEntity;
class Scene;

// 只读物理查询视图（最小占位实现）：
// 实际项目中应由 Scene 在每帧物理步后填充缓存数据。
struct PhysicsQuery {
    // 指向"本帧触发器重叠映射"：triggerEntity -> { other entity ids }
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

// 实体查询接口（只读）
struct EntityQuery {
    const std::unordered_map<EntityId, IEntity *> *entityMap = nullptr;

    // 根据 ID 获取实体指针（只读）
    IEntity *getEntity(EntityId id) const {
        if (!entityMap) return nullptr;
        auto it = entityMap->find(id);
        return (it != entityMap->end()) ? it->second : nullptr;
    }

    // 检查实体是否存在
    bool exists(EntityId id) const {
        return entityMap && entityMap->find(id) != entityMap->end();
    }

    // 获取所有实体 ID（用于遍历）
    void getAllEntityIds(std::vector<EntityId> &out) const {
        out.clear();
        if (!entityMap) return;
        out.reserve(entityMap->size());
        for (const auto &kv: *entityMap) {
            // 解引用指针
            out.push_back(kv.first);
        }
    }

    // 获取实体数量
    size_t count() const {
        return entityMap ? entityMap->size() : 0;
    }
};

// 命令缓冲区：支持通用实体生成和销毁
struct CommandBuffer {
    // 通用实体生成命令
    struct SpawnEntityCmd {
        std::function<std::unique_ptr<IEntity>(Scene *)> factory; // 工厂函数
        std::function<void(IEntity *)> configurator; // 配置回调
    };

    // 销毁命令
    struct DestroyCmd {
        EntityId id;
    };

    // 场景重置命令
    struct ResetCmd {
        bool doReset = false;
    };

    std::vector<SpawnEntityCmd> spawnEntities; // 通用生成队列
    std::vector<DestroyCmd> toDestroy;
    ResetCmd reset;

    // 通用生成方法（类型安全，带配置回调）
    template<typename EntityType, typename ConfigFunc>
    void spawn(ConfigFunc &&configurator) {
        SpawnEntityCmd cmd;
        cmd.factory = [](Scene *scene) -> std::unique_ptr<IEntity> {
            return std::make_unique<EntityType>();
        };
        // 捕获配置函数，并在执行时安全转换类型
        cmd.configurator = [config = std::forward<ConfigFunc>(configurator)](IEntity *e) {
            config(static_cast<EntityType *>(e));
        };
        spawnEntities.push_back(std::move(cmd));
    }

    // 无配置版本
    template<typename EntityType>
    void spawn() {
        spawn<EntityType>([](EntityType *) {
        });
    }

    // 便捷方法：生成球体（保持兼容性）
    void spawnBall(const DirectX::XMFLOAT3 &pos, float radius);

    // 销毁实体
    void destroyEntity(EntityId e) { toDestroy.push_back({e}); }

    // 重置场景
    void resetScene() { reset.doReset = true; }

    // 清空所有命令
    void clear() {
        spawnEntities.clear();
        toDestroy.clear();
        reset.doReset = false;
    }
};

// 每帧注入到实体 update/onCollision 的上下文
struct WorldContext {
    float time = 0.0f;
    float dt = 0.0f;
    const PhysicsQuery *physics = nullptr; // 只读物理查询
    const EntityQuery *entities = nullptr; // 只读实体查询
    CommandBuffer *commands = nullptr; // 可写命令缓冲
};
