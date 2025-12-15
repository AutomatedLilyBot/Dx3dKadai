#pragma once
#include "DynamicEntity.hpp"
#include "BlockEntity.hpp"
#include <memory>

#include "core/resource/ResourceManager.hpp"

enum class NodeTeam {
    Friendly,
    Enemy,
    Neutral
};

auto clampDirToXZ(const DirectX::XMFLOAT3 &dir);

class BulletEntity : public DynamicEntity {
public:
    BulletEntity() = default;

    NodeTeam team = NodeTeam::Friendly;
    float power = 1.0f;
    float minSpeedToLive = 0.5f;  // 降低阈值，更快删除
    float lifetime = 10.0f;        // 最大生命周期（秒）
    float timeAlive = 0.0f;        // 已存活时间

    // 发射者 ID（用于忽略碰撞）
    EntityId shooterId = 0;
    float ignoreShooterTime = 0.3f;  // 发射后忽略发射者的时间（秒）

    // 使用 ResourceManager 初始化（推荐）
    void initialize(float radius, const std::wstring &modelPath, ResourceManager *resMgr);

    // 旧版本：直接使用 Device 加载（已废弃，保留兼容性）
    void initialize(float radius, const std::wstring &modelPath, ID3D11Device *dev);
    void update(WorldContext &ctx, float dt) override;
    void onCollision(WorldContext &ctx, EntityId other, TriggerPhase phase, const OverlapResult &c) override;

    void bounceFromSurface(const DirectX::XMFLOAT3 &normal);

private:
    std::unique_ptr<Model> modelOwned;
};
