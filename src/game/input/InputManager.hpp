#pragma once
#include <DirectXMath.h>
#include <optional>
#include "../runtime/Scene.hpp"
#include "../../core/gfx/Camera.hpp"

struct Ray {
    DirectX::XMFLOAT3 origin;
    DirectX::XMFLOAT3 dir;
};

class InputManager {
public:
    Ray screenPointToRay(int screenX, int screenY, const Camera &camera);
    EntityId raycastEntities(const Ray &ray, const Scene &scene, float maxDist = 100.0f);
    bool raycastPlane(const Ray &ray, float planeY, DirectX::XMFLOAT3 &hitPoint);

    void updateMouseButtons(float dt);
    void setRightButtonDown(bool down) { rightButtonDown_ = down; }
    bool isRightClickShort() const { return rightClickShort_; }
    bool isRightClickLong() const { return rightClickLong_; }

    void selectNode(EntityId id) { selectedNode_ = id; }
    void deselectNode() { selectedNode_.reset(); }
    std::optional<EntityId> selectedNode() const { return selectedNode_; }

private:
    float rightButtonHoldTime_ = 0.0f;
    bool rightButtonDown_ = false;
    bool rightClickShort_ = false;
    bool rightClickLong_ = false;
    float rightLongPressThreshold_ = 0.3f;
    std::optional<EntityId> selectedNode_;
};
