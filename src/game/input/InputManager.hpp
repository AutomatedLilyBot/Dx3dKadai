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

    // UI交互支持：鼠标位置和按钮状态
    void setMousePosition(int x, int y) { mouseX_ = x; mouseY_ = y; }
    void getMousePosition(int& x, int& y) const { x = mouseX_; y = mouseY_; }

    // 归一化鼠标坐标（0-1范围，用于UI元素）
    void getMousePositionNormalized(float& x, float& y, int screenWidth, int screenHeight) const {
        x = static_cast<float>(mouseX_) / static_cast<float>(screenWidth);
        y = static_cast<float>(mouseY_) / static_cast<float>(screenHeight);
    }

    void setLeftButtonDown(bool down) { leftButtonDown_ = down; }
    bool isLeftButtonDown() const { return leftButtonDown_; }
    bool isLeftButtonPressed() const { return leftButtonPressed_; }

private:
    float rightButtonHoldTime_ = 0.0f;
    bool rightButtonDown_ = false;
    bool rightClickShort_ = false;
    bool rightClickLong_ = false;
    float rightLongPressThreshold_ = 0.3f;
    std::optional<EntityId> selectedNode_;

    // UI交互状态
    int mouseX_ = 0;
    int mouseY_ = 0;
    bool leftButtonDown_ = false;
    bool leftButtonPressed_ = false;  // 本帧是否按下（edge-triggered）
    bool leftButtonWasDown_ = false;  // 上一帧状态
};
