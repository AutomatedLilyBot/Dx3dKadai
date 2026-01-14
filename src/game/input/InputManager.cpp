#include "InputManager.hpp"
#include <DirectXMath.h>

using namespace DirectX;

Ray InputManager::screenPointToRay(int screenX, int screenY, const Camera &camera) {
    // Assume viewport dimensions are available; replace with actual values as needed
    const float viewportWidth = 1280.0f;  // TODO: Replace with actual viewport width
    const float viewportHeight = 720.0f;  // TODO: Replace with actual viewport height

    // Convert screen (pixel) coordinates to normalized device coordinates (NDC)
    float ndcX = (2.0f * screenX) / viewportWidth - 1.0f;
    float ndcY = 1.0f - (2.0f * screenY) / viewportHeight; // Y is inverted

    // Prepare points in NDC at near and far planes
    XMVECTOR nearPointNDC = XMVectorSet(ndcX, ndcY, 0.0f, 1.0f);
    XMVECTOR farPointNDC  = XMVectorSet(ndcX, ndcY, 1.0f, 1.0f);

    // Get view and projection matrices from camera
    XMMATRIX view = camera.getViewMatrix();
    float aspectRatio = viewportWidth / viewportHeight;
    XMMATRIX proj = camera.getProjectionMatrix(aspectRatio);
    XMMATRIX invViewProj = XMMatrixInverse(nullptr, view * proj);

    // Unproject to world space
    XMVECTOR nearPointWorld = XMVector3TransformCoord(nearPointNDC, invViewProj);
    XMVECTOR farPointWorld  = XMVector3TransformCoord(farPointNDC, invViewProj);

    // Ray origin is near point, direction is (far - near)
    XMVECTOR dir = XMVector3Normalize(farPointWorld - nearPointWorld);

    Ray r{};
    XMStoreFloat3(&r.origin, nearPointWorld);
    XMStoreFloat3(&r.dir, dir);
    return r;
}

EntityId InputManager::raycastEntities(const Ray &ray, const Scene &scene, float maxDist) {
    EntityId closestEntity = 0;
    float closestDistance = maxDist;

    // 获取场景中的所有实体
    const auto* entityMap = scene.getEntityMap();
    if (!entityMap) {
        printf("[Raycast] EntityMap is null!\n");
        return 0;
    }

    printf("[Raycast] Testing %zu entities...\n", entityMap->size());
    int testedColliders = 0;
    bool debugFirstFew = true;

    // 遍历所有实体，检测射线相交
    for (const auto& [id, entity] : *entityMap) {
        if (!entity) continue;

        // 遍历实体的所有碰撞体
        auto colliders = entity->colliders();
        for (auto* collider : colliders) {
            if (!collider) continue;
            // 跳过触发器（只检测实体本体）
            if (collider->isTrigger()) continue;

            testedColliders++;

            // Debug前几个碰撞体的信息
            if (debugFirstFew && testedColliders <= 3) {
                DirectX::XMFLOAT3 colPos = collider->ownerWorldPosition();
                printf("  Testing collider %d: type=%d, pos=(%.2f, %.2f, %.2f)\n",
                       testedColliders, (int)collider->kind(), colPos.x, colPos.y, colPos.z);
            }

            float distance = 0.0f;
            if (collider->intersectsRay(ray.origin, ray.dir, distance)) {
                printf("  [HIT] Entity %llu at distance %.2f\n", id, distance);
                if (distance < closestDistance) {
                    closestDistance = distance;
                    closestEntity = id;
                }
            }
        }

        if (testedColliders >= 3) debugFirstFew = false;
    }

    printf("[Raycast] Tested %d colliders, result: entity %llu at %.2f\n",
           testedColliders, closestEntity, closestDistance);

    return closestEntity;
}

bool InputManager::raycastPlane(const Ray &ray, float planeY, DirectX::XMFLOAT3 &hitPoint) {
    if (ray.dir.y == 0) return false;
    float t = (planeY - ray.origin.y) / ray.dir.y;
    if (t < 0) return false;
    hitPoint = {ray.origin.x + ray.dir.x * t, planeY, ray.origin.z + ray.dir.z * t};
    return true;
}

void InputManager::updateMouseButtons(float dt) {
    rightClickShort_ = false;
    rightClickLong_ = false;
    if (rightButtonDown_) {
        rightButtonHoldTime_ += dt;
        if (rightButtonHoldTime_ > rightLongPressThreshold_) {
            rightClickLong_ = true;
        }
    } else {
        if (rightButtonHoldTime_ > 0 && rightButtonHoldTime_ <= rightLongPressThreshold_) {
            rightClickShort_ = true;
        }
        rightButtonHoldTime_ = 0.0f;
    }

    // 更新左键按下状态（edge-triggered）
    leftButtonPressed_ = leftButtonDown_ && !leftButtonWasDown_;
    leftButtonWasDown_ = leftButtonDown_;
}
