#pragma once
#include "Scene.hpp"
#include "../../core/gfx/Texture.hpp"

class MenuScene : public Scene {
public:
    MenuScene() = default;
    ~MenuScene() override = default;

    void init(Renderer *renderer) override;
    void tick(float dt) override;
    void handleInput(float dt, const void *window) override;
    void render() override;

    bool exitRequested() const { return exitRequested_; }

private:
    Texture background_;
    Texture title_;
    Texture startButton_;
    Texture exitButton_;

    bool startRequested_ = false;
    bool exitRequested_ = false;
    bool enterWasPressed_ = false;
    bool escWasPressed_ = false;
};
