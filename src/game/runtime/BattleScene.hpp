#pragma once
#include "Scene.hpp"
#include "../entity/BlockEntity.hpp"
#include "../entity/NodeEntity.hpp"
#include "../../core/resource/ResourceManager.hpp"

class BattleScene : public Scene {
public:
    BattleScene() = default;
    ~BattleScene() override = default;
    void init(Renderer *renderer) override;

private:
    ResourceManager resourceManager_;
    void createField();
    void createNodes();
};
