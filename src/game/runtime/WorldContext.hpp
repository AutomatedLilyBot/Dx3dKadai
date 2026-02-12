#pragma once
#pragma execution_character_set("utf-8")

#include <vector>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>
#include <DirectXMath.h>
#include "core/gfx/Renderer.hpp"
#include "../src/core/physics/PhysicsWorld.hpp"

// 前置声明
struct IEntity;
class Scene;
class ResourceManager;

// 只读物理查询视图：
// 由 Scene 在每帧物理步后填充，提供触发器重叠状态的快速查询。
struct PhysicsQuery {
    // 指向"本帧触发器重叠映射"：entityId -> { 与之重叠的其他实体 ids }
    //
    // 重要：此映射仅包含"涉及至少一个 Trigger 碰撞体"的碰撞对。
    // - 如果实体 A 有触发器，实体 B 与 A 重叠，则 triggerOverlaps[A] 包含 B
    // - 如果实体 B 也有触发器，则 triggerOverlaps[B] 也包含 A（双向记录）
    // - 如果 A 和 B 都没有触发器（纯实体碰撞），则不会出现在此映射中
    const std::unordered_map<EntityId, std::unordered_set<EntityId> > *triggerOverlaps = nullptr;

    // 检查指定实体是否有任何触发器正在与其他实体重叠
    // 参数 e: 要查询的实体 ID（可以是拥有触发器的实体，也可以是与触发器重叠的实体）
    // 返回: 如果该实体在触发器重叠映射中且有至少一个重叠对象，返回 true
    bool isAnyTriggerOverlapping(EntityId e) const {
        if (!triggerOverlaps) return false;
        auto it = triggerOverlaps->find(e);
        return it != triggerOverlaps->end() && !it->second.empty();
    }

    // 检查两个特定实体之间是否存在触发器重叠
    // 参数 triggerEntity: 拥有触发器的实体（或参与触发碰撞的任一方）
    // 参数 withEntity: 另一个实体
    // 返回: 如果这两个实体之间存在触发器重叠，返回 true
    bool isTriggerOverlapping(EntityId triggerEntity, EntityId withEntity) const {
        if (!triggerOverlaps) return false;
        auto it = triggerOverlaps->find(triggerEntity);
        if (it == triggerOverlaps->end()) return false;
        return it->second.find(withEntity) != it->second.end();
    }

    // 获取所有与指定实体的触发器重叠的其他实体列表
    // 参数 triggerEntity: 要查询的实体 ID（通常是拥有触发器碰撞体的实体）
    // 参数 out: 输出向量，将填充所有与该实体重叠的其他实体 ID
    // 返回: 重叠实体的数量
    //
    // 用途示例：
    // - SpawnerEntity 检查生成区域是否被占用
    // - NodeEntity 检查发射区域内是否有敌方实体
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
    ResourceManager *resources = nullptr; // 资源管理器（用于加载模型/纹理）
    Renderer *currentrenderer = nullptr; // 当前渲染器
    class Camera *camera = nullptr; // 相机指针（用于触发抖动等效果）
};