#pragma once
#include "../runtime/Scene.hpp"
#include "../../core/gfx/Texture.hpp"

class HelpScene : public Scene {
public:
    HelpScene() = default;
    ~HelpScene() override = default;

    void init(Renderer *renderer) override;
    void tick(float dt) override;
    void handleInput(float dt, const void *window) override;

private:
    // 纹理资源
    Texture helpImage_;

    bool returnRequested_ = false;
    bool escWasPressed_ = false;
};
