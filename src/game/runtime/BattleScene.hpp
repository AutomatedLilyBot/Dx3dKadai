#pragma once
#include "Scene.hpp"
#include "../entity/BlockEntity.hpp"
#include "../entity/NodeEntity.hpp"
#include "../../core/resource/ResourceManager.hpp"
#include "../../core/gfx/Camera.hpp"
#include "../input/InputManager.hpp"
#include <SFML/Window.hpp>

class BattleScene : public Scene {
public:
    BattleScene() = default;
    ~BattleScene() override = default;
    void init(Renderer *renderer) override;
    void tick(float dt);  // 隐藏基类的 tick()，在调用基类前先更新 camera
    void handleInput(float dt, const void* window) override;

    // 返回 ResourceManager 供实体使用
    ResourceManager *getResourceManager() override { return &resourceManager_; }

    // 访问 Camera
    Camera& camera() { return camera_; }
    const Camera& camera() const { return camera_; }

    // 访问 InputManager
    InputManager& inputManager() { return inputManager_; }

private:
    ResourceManager resourceManager_;
    Camera camera_;  // 场景管理的 Camera
    InputManager inputManager_;  // 输入管理器

    // 当前选中的 Node（用于控制）
    EntityId selectedNodeId_ = 0;

    void createField();
    void createNodes();

    // 辅助函数：获取 Node 实体指针
    NodeEntity* getNodeEntity(EntityId id);
};
