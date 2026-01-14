#pragma once
#include "UIImage.hpp"
#include <functional>

// 按钮状态
enum class ButtonState {
    Normal,
    Hovered,
    Pressed
};

// UIButton: 可交互按钮元素
class UIButton : public UIImage {
public:
    UIButton() = default;
    virtual ~UIButton() = default;

    void init() override;
    void update(float dt) override;

    // 设置交互回调
    void setOnClick(std::function<void()> callback) { onClick_ = callback; }
    void setOnHover(std::function<void()> callback) { onHover_ = callback; }

    // 配置hover效果
    void setHoverScale(float scale) { hoverScale_ = scale; }
    void setHoverTint(const DirectX::XMFLOAT4& tint) { hoverTint_ = tint; }
    void setNormalTint(const DirectX::XMFLOAT4& tint) { normalTint_ = tint; }

    // 访问状态
    ButtonState state() const { return state_; }

    // 设置InputManager和屏幕尺寸（需要外部设置）
    void setInputManager(class InputManager* inputManager) { inputManager_ = inputManager; }
    void setScreenSize(int width, int height) { screenWidth_ = width; screenHeight_ = height; }

private:
    ButtonState state_ = ButtonState::Normal;
    ButtonState lastState_ = ButtonState::Normal;

    // 交互效果配置
    float hoverScale_ = 1.1f;
    DirectX::XMFLOAT4 normalTint_{1.0f, 1.0f, 1.0f, 1.0f};
    DirectX::XMFLOAT4 hoverTint_{1.2f, 1.2f, 1.2f, 1.0f};  // 稍微变亮

    // 回调函数
    std::function<void()> onClick_;
    std::function<void()> onHover_;

    // 外部依赖（弱引用）
    class InputManager* inputManager_ = nullptr;
    int screenWidth_ = 1280;
    int screenHeight_ = 720;
};
