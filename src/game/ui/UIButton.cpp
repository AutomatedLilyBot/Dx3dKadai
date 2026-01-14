#include "UIButton.hpp"
#include "../input/InputManager.hpp"

void UIButton::init() {
    UIImage::init();
    // 初始化为Normal状态
    state_ = ButtonState::Normal;
    transform_.scaleX = 1.0f;
    transform_.scaleY = 1.0f;
    setTint(normalTint_);
}

void UIButton::update(float dt) {
    UIImage::update(dt);

    if (!inputManager_) return;

    // 获取归一化鼠标位置
    float mouseX, mouseY;
    inputManager_->getMousePositionNormalized(mouseX, mouseY, screenWidth_, screenHeight_);

    // 检测鼠标悬停
    bool isHovered = containsPoint(mouseX, mouseY);
    bool isLeftDown = inputManager_->isLeftButtonDown();
    bool isLeftPressed = inputManager_->isLeftButtonPressed();

    // 状态机转换
    lastState_ = state_;

    if (isHovered) {
        if (isLeftDown) {
            state_ = ButtonState::Pressed;
        } else {
            state_ = ButtonState::Hovered;
            // 如果上一帧是Pressed，这一帧松开，触发onClick
            if (lastState_ == ButtonState::Pressed && onClick_) {
                onClick_();
            }
        }
    } else {
        state_ = ButtonState::Normal;
    }

    // 应用视觉效果
    switch (state_) {
        case ButtonState::Normal:
            transform_.scaleX = 1.0f;
            transform_.scaleY = 1.0f;
            setTint(normalTint_);
            break;

        case ButtonState::Hovered:
            transform_.scaleX = hoverScale_;
            transform_.scaleY = hoverScale_;
            setTint(hoverTint_);
            // 触发hover回调（仅在进入时触发一次）
            if (lastState_ != ButtonState::Hovered && onHover_) {
                onHover_();
            }
            break;

        case ButtonState::Pressed:
            transform_.scaleX = hoverScale_ * 0.95f;  // 按下时稍微缩小
            transform_.scaleY = hoverScale_ * 0.95f;
            setTint(DirectX::XMFLOAT4{0.8f, 0.8f, 0.8f, 1.0f});  // 按下时变暗
            break;
    }
}
