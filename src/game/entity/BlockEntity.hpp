#pragma once
#include "StaticEntity.hpp"

class BlockEntity : public StaticEntity {
public:
    enum class ResponseType {
        None,
        DestroyBullet,
        BounceBullet,
        SpecialEvent
    };

    ResponseType responseType = ResponseType::None;

    void onCollision(WorldContext &ctx, EntityId other, TriggerPhase phase, const OverlapResult &c) override;
};
