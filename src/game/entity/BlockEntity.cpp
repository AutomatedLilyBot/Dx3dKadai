#include "BlockEntity.hpp"
#include "BulletEntity.hpp"
#include "game/runtime/WorldContext.hpp"

void BlockEntity::onCollision(WorldContext &ctx, EntityId other, TriggerPhase phase, const OverlapResult &c) {
    if (phase != TriggerPhase::Enter) return;
    auto *otherEntity = ctx.entities->getEntity(other);
    if (!otherEntity) return;
    if (auto *bullet = dynamic_cast<BulletEntity *>(otherEntity)) {
        switch (responseType) {
            case ResponseType::DestroyBullet:
                ctx.commands->destroyEntity(bullet->id());
                break;
            case ResponseType::BounceBullet:
                bullet->bounceFromSurface(c.normal);
                break;
            case ResponseType::SpecialEvent:
            case ResponseType::None:
            default:
                break;
        }
    }
}
