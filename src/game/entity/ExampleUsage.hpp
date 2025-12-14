#pragma once
#include "DynamicEntity.hpp"
#include "../runtime/WorldContext.hpp"

// 示例：演示如何在实体中查询其他实体
class ExampleEntity : public DynamicEntity {
public:
    void update(WorldContext &ctx, float dt) override {
        // 1. 查询特定实体
        EntityId targetId = 42; // 假设我们知道目标 ID
        if (IEntity *target = ctx.entities->getEntity(targetId)) {
            // 获取目标的位置
            DirectX::XMFLOAT3 targetPos = target->transformRef().position;

            // 计算距离并做一些逻辑
            DirectX::XMFLOAT3 myPos = transform.position;
            float dx = targetPos.x - myPos.x;
            float dy = targetPos.y - myPos.y;
            float dz = targetPos.z - myPos.z;
            float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

            if (dist < 1.0f) {
                // 靠得很近，做些什么...
            }
        }

        // 2. 遍历所有实体
        std::vector<EntityId> allIds;
        ctx.entities->getAllEntityIds(allIds);
        for (EntityId otherId: allIds) {
            if (otherId == id()) continue; // 跳过自己

            IEntity *other = ctx.entities->getEntity(otherId);
            if (!other) continue;

            // 对每个其他实体做处理...
        }

        // 3. 检查实体是否存在
        if (ctx.entities->exists(123)) {
            // 实体 123 存在
        }

        // 4. 获取实体总数
        size_t entityCount = ctx.entities->count();
    }

    void onCollision(WorldContext &ctx, EntityId other, TriggerPhase phase,
                     const OverlapResult &contact) override {
        // 在碰撞回调中也可以查询实体
        if (IEntity *otherEntity = ctx.entities->getEntity(other)) {
            // 获取碰撞对象的信息
            DirectX::XMFLOAT3 otherPos = otherEntity->transformRef().position;

            if (phase == TriggerPhase::Enter) {
                // 刚开始接触，可以获取对方的 RigidBody 信息
                if (RigidBody *otherRb = otherEntity->rigidBody()) {
                    // 访问对方的速度、质量等
                    DirectX::XMFLOAT3 otherVel = otherRb->velocity;
                }
            }
        }
    }
};
