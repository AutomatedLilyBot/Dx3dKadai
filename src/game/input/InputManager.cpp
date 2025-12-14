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
    XMMATRIX proj = camera.getProjectionMatrix();
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

EntityId InputManager::raycastEntities(const Ray &/*ray*/, const Scene &/*scene*/, float /*maxDist*/) {
    return 0;
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
}
