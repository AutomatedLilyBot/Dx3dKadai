#pragma once
#pragma execution_character_set("utf-8")

#include "Scene.hpp"
#include "core/gfx/Model.hpp"

// PlayScene：演示场景，包含平台、生成器和球
class PlayScene : public Scene {
public:
    PlayScene() = default;
    ~PlayScene() override = default;

    // 初始化场景：创建平台和生成器
    void init(Renderer *renderer) override;

private:
    // 场景特定资源
    Model platformModel_;
    bool platformModelLoaded_ = false;
};
