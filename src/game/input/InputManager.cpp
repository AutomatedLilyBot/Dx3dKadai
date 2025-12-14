#include "InputManager.hpp"
#include <DirectXMath.h>

using namespace DirectX;

Ray InputManager::screenPointToRay(int /*screenX*/, int /*screenY*/, const Camera &camera) {
    Ray r{};
    r.origin = camera.position();
    r.dir = camera.forward();
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
