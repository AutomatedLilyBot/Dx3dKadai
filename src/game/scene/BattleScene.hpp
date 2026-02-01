#pragma once
#include "../runtime/Scene.hpp"
#include "../entity/BlockEntity.hpp"
#include "../entity/NodeEntity.hpp"
#include "../../core/resource/ResourceManager.hpp"
#include "../../core/gfx/Camera.hpp"
#include "../input/InputManager.hpp"
#include <SFML/Window.hpp>

#include "game/ui/UINumberDisplay.hpp"

class BattleScene : public Scene {
public:
    BattleScene() = default;

    ~BattleScene() override = default;

    void init(Renderer *renderer) override;

    void tick(float dt); // 隐藏基类的 tick()，在调用基类前先更新 camera
    void handleInput(float dt, const void *window) override;

    // 返回 ResourceManager 供实体使用
    ResourceManager *getResourceManager() override { return &resourceManager_; }

    // 访问 Camera
    Camera &camera() { return camera_; }
    const Camera &camera() const { return camera_; }

    // 访问 InputManager
    InputManager &inputManager() { return inputManager_; }

    // 访问选中的 Node ID
    EntityId selectedNodeId() const { return selectedNodeId_; }

    // 访问 Node 实体（公开给外部使用）
    NodeEntity *getNodeEntity(EntityId id);

    // 重写 Scene 的虚函数以支持 Billboard 渲染
    const Camera *getCameraForRendering() const override { return &camera_; }
    Camera *getCameraForShake() override { return &camera_; }

    bool isBillboard(IEntity *entity) const override;

    void updateBillboardOrientation(IEntity *entity, const Camera *camera) override;

    // 重写 render 方法以绘制 Node 指示箭头
    void render() override;

private:
    Camera camera_; // 场景管理的 Camera
    InputManager inputManager_; // 输入管理器

    // 当前选中的 Node（用于控制）
    EntityId selectedNodeId_ = 0;

    // 演示模式
    bool isDemoMode_ = false;
    void enterDemoMode();
    void exitDemoMode();

    // 上方显示区的数字元素的指针
    UINumberDisplay *totalNodeCount_;
    UINumberDisplay *friendlyNodeCount_;
    UINumberDisplay *enemyNodeCount_;

    // 下方详情区的数字元素的指针

    UINumberDisplay *selectedNodeHealth_;
    UINumberDisplay *selectedNodePower_;
    UINumberDisplay *selectedNodeFireInterval_;

    ResourceManager resourceManager_;

    void createField();

    void createNodes();

    void createUI();
};