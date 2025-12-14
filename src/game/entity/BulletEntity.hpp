#pragma once
#include "DynamicEntity.hpp"
#include "BlockEntity.hpp"
#include <memory>

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
    float minSpeedToLive = 1.0f;

    void initialize(float radius, const std::wstring &modelPath, ID3D11Device *dev);
    void update(WorldContext &ctx, float dt) override;
    void onCollision(WorldContext &ctx, EntityId other, TriggerPhase phase, const OverlapResult &c) override;

    void bounceFromSurface(const DirectX::XMFLOAT3 &normal);

private:
    std::unique_ptr<Model> modelOwned;
};
