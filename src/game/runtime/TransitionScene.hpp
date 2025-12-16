#pragma once
#include "Scene.hpp"
#include "../../core/gfx/Texture.hpp"
#include <memory>

class TransitionScene : public Scene {
public:
    explicit TransitionScene(std::unique_ptr<Scene> next);
    ~TransitionScene() override = default;

    void init(Renderer *renderer) override;
    void tick(float dt) override;
    void render() override;

private:
    std::unique_ptr<Scene> nextScene_;
    float timer_ = 0.0f;
    float duration_ = 0.5f;
    Texture loadingTexture_;
};
