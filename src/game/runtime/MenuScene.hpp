#pragma once
#include "Scene.hpp"
#include "../../core/resource/ResourceManager.hpp"
#include "../../core/gfx/Texture.hpp"

class MenuScene : public Scene {
public:
    void init(Renderer *renderer) override;
    void render() override;
    void onUpdate(WorldContext &, float) override {}

private:
    ResourceManager resourceManager_{};
    Texture *backgroundTex_ = nullptr;
};
