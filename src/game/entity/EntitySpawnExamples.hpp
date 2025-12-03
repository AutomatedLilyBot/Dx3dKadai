#pragma once
#pragma execution_character_set("utf-8")

/**
 * 通用实体生成系统使用示例
 *
 * 本文件演示如何使用新的 spawn() 方法替代旧的 spawnBall()，
 * 以及如何生成任意类型的实体并进行配置。
 */

#include "DynamicEntity.hpp"
#include "BallEntity.hpp"
#include "../runtime/WorldContext.hpp"

// ============================================================================
// 示例 1：简单生成（无需配置）
// ============================================================================
void Example1_SimpleSpawn(WorldContext &ctx) {
    // 使用默认参数生成实体
    ctx.commands->spawn<BallEntity>();
}

// ============================================================================
// 示例 2：带配置的生成（推荐方式）
// ============================================================================
void Example2_SpawnWithConfig(WorldContext &ctx) {
    ctx.commands->spawn<BallEntity>([](BallEntity *ball) {
        // 设置位置
        ball->transform.position = {0, 5, 0};

        // 设置物理属性
        ball->rb.velocity = {1, 0, 0}; // 初始速度
        ball->rb.invMass = 0.5f; // 质量=2
        ball->rb.restitution = 0.8f; // 高弹性

        // 设置缩放
        ball->transform.scale = {0.5f, 0.5f, 0.5f};

        // 自定义调试颜色
        ball->collider->setDebugColor(DirectX::XMFLOAT4(1, 0, 0, 1)); // 红色
    });
}

// ============================================================================
// 示例 3：使用便捷方法（保持兼容性）
// ============================================================================
void Example3_ConvenienceMethod(WorldContext &ctx) {
    // 旧的 API 仍然可用
    ctx.commands->spawnBall({0, 3, 0}, 0.25f);
}

// ============================================================================
// 示例 4：在实体内部生成子弹/投射物
// ============================================================================
class ShooterEntity : public DynamicEntity {
public:
    void update(WorldContext &ctx, float dt) override {
        // 按某个条件生成投射物
        if (shouldShoot) {
            ctx.commands->spawn<BallEntity>([this](BallEntity *projectile) {
                // 从自己的位置生成
                projectile->transform.position = this->transform.position;

                // 朝向某个方向发射
                DirectX::XMFLOAT3 shootDir = {1, 0, 0};
                projectile->rb.velocity = {
                    shootDir.x * shootSpeed,
                    shootDir.y * shootSpeed,
                    shootDir.z * shootSpeed
                };

                // 小一点
                projectile->transform.scale = {0.1f, 0.1f, 0.1f};
            });

            shouldShoot = false;
        }
    }

private:
    bool shouldShoot = false;
    float shootSpeed = 10.0f;
};

// ============================================================================
// 示例 5：访问其他实体并基于其状态生成
// ============================================================================
void Example5_SpawnRelativeToOther(WorldContext &ctx, EntityId targetId) {
    // 查询目标实体
    IEntity *target = ctx.entities->getEntity(targetId);
    if (!target) return;

    // 在目标上方生成一个球
    DirectX::XMFLOAT3 targetPos = target->transformRef().position;

    ctx.commands->spawn<BallEntity>([targetPos](BallEntity *ball) {
        ball->transform.position = {
            targetPos.x,
            targetPos.y + 2.0f, // 上方2米
            targetPos.z
        };

        // 给一个初始下落速度
        ball->rb.velocity = {0, -1, 0};
    });
}

// ============================================================================
// 示例 6：生成自定义实体类型
// ============================================================================
class EnemyEntity : public DynamicEntity {
public:
    int health = 100;
    float attackDamage = 10.0f;

    void setHealth(int h) { health = h; }
    void setAttackDamage(float dmg) { attackDamage = dmg; }
};

void Example6_SpawnCustomEntity(WorldContext &ctx) {
    ctx.commands->spawn<EnemyEntity>([](EnemyEntity *enemy) {
        enemy->transform.position = {5, 0, 5};
        enemy->setHealth(150);
        enemy->setAttackDamage(25.0f);
        enemy->rb.invMass = 2.0f; // 质量=0.5
    });
}

// ============================================================================
// 示例 7：批量生成
// ============================================================================
void Example7_BatchSpawn(WorldContext &ctx) {
    // 生成一排球
    for (int i = 0; i < 10; ++i) {
        ctx.commands->spawn<BallEntity>([i](BallEntity *ball) {
            ball->transform.position = {
                static_cast<float>(i) * 0.6f, // 间隔0.6米
                5.0f,
                0.0f
            };

            // 每个球颜色不同
            float hue = static_cast<float>(i) / 10.0f;
            ball->collider->setDebugColor(DirectX::XMFLOAT4(hue, 1 - hue, 0.5f, 1));
        });
    }
}

// ============================================================================
// 示例 8：在碰撞回调中生成实体
// ============================================================================
class BombEntity : public DynamicEntity {
public:
    void onCollision(WorldContext &ctx, EntityId other, TriggerPhase phase,
                     const OverlapResult &contact) override {
        if (phase == TriggerPhase::Enter) {
            // 碰撞时生成碎片
            for (int i = 0; i < 5; ++i) {
                ctx.commands->spawn<BallEntity>([this, i](BallEntity *fragment) {
                    fragment->transform.position = this->transform.position;

                    // 随机方向飞出
                    float angle = (static_cast<float>(i) / 5.0f) * 3.14159f * 2.0f;
                    fragment->rb.velocity = {
                        std::cos(angle) * 3.0f,
                        2.0f,
                        std::sin(angle) * 3.0f
                    };

                    // 小碎片
                    fragment->transform.scale = {0.1f, 0.1f, 0.1f};
                });
            }

            // 销毁自己
            ctx.commands->destroyEntity(id());
        }
    }
};

// ============================================================================
// 使用总结
// ============================================================================

/*
 * 新系统优势：
 *
 * 1. 类型安全：spawn<T>() 自动推断类型，无需手动转换
 * 2. 灵活配置：lambda 回调可以设置任意属性
 * 3. 通用性：支持任意实体类型，不仅限于球体
 * 4. 访问上下文：配置回调可以访问 WorldContext 和其他实体
 * 5. 保持兼容：旧的 spawnBall() 仍然可用
 *
 * 对比：
 *
 * 旧方式：
 *   ctx.commands->spawnBall({0, 3, 0}, 0.25f);
 *   // 只能设置位置和半径，无法定制速度、质量等
 *
 * 新方式：
 *   ctx.commands->spawn<BallEntity>([](BallEntity* ball) {
 *       ball->transform.position = {0, 3, 0};
 *       ball->rb.velocity = {1, 0, 0};      // 可以设置速度
 *       ball->rb.invMass = 0.5f;            // 可以设置质量
 *       ball->collider->setDebugColor(...); // 可以设置颜色
 *       // ... 任意属性
 *   });
 */