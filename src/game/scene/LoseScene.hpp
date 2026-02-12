#pragma once
#include "../runtime/Scene.hpp"
#include "../../core/gfx/Texture.hpp"
#include "../input/InputManager.hpp"
#include "game/ui/UIButton.hpp"

class LoseScene : public Scene {
public:
    LoseScene() = default;
    ~LoseScene() override = default;

    void init(Renderer *renderer) override;
    void tick(float dt) override;
    void handleInput(float dt, const void *window) override;

private:
    // 纹理资源
    Texture background_;
    Texture returnButton_;

    // UI元素指针
    UIButton* returnButtonUI_ = nullptr;

    // 输入管理器
    InputManager inputManager_;

    bool returnRequested_ = false;
};
