#pragma once
#include "../runtime/Scene.hpp"
#include "../../core/gfx/Texture.hpp"
#include "../input/InputManager.hpp"
#include "game//ui/UIButton.hpp"

class MenuScene : public Scene {
public:
    MenuScene() = default;
    ~MenuScene() override = default;

    void init(Renderer *renderer) override;
    void tick(float dt) override;
    void handleInput(float dt, const void *window) override;

    bool exitRequested() const { return exitRequested_; }

private:
    // 纹理资源（UI元素会引用这些纹理）
    Texture background_;
    Texture title_;
    Texture startButton_;
    Texture helpButton_;
    Texture exitButton_;

    // UI元素指针（用于回调访问，生命周期由Scene管理）
    UIButton* startButtonUI_ = nullptr;
    UIButton* helpButtonUI_ = nullptr;
    UIButton* exitButtonUI_ = nullptr;

    // 输入管理器
    InputManager inputManager_;

    bool startRequested_ = false;
    bool helpRequested_ = false;
    bool exitRequested_ = false;
    bool enterWasPressed_ = false;
    bool escWasPressed_ = false;
};
