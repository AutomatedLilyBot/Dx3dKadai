#pragma once
#include "Scene.hpp"
#include "../../core/resource/ResourceManager.hpp"
#include "../../core/gfx/Texture.hpp"
#include <memory>

class TransitionScene : public Scene {
public:
    explicit TransitionScene(std::unique_ptr<Scene> target, float waitSeconds = 1.0f);
    void init(Renderer *renderer) override;
    void onUpdate(WorldContext &, float dt) override;
    void render() override;

    bool isComplete() const { return readyForNext_; }
    std::unique_ptr<Scene> takeNextScene();

private:
    ResourceManager resourceManager_{};
    std::unique_ptr<Scene> targetScene_{};
    float elapsed_ = 0.0f;
    float waitTime_ = 1.0f;
    bool readyForNext_ = false;
    Texture *loadingTexture_ = nullptr;
};
