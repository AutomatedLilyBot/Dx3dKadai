#pragma once
#include "DynamicEntity.hpp"
#include "../../core/gfx/ModelLoader.hpp"
#include <memory>

// Ball 动态实体：持有一个 SphereCollider，加载一个模型用于渲染（临时用 cube.fbx）
class BallEntity : public DynamicEntity {
public:
    explicit BallEntity(ID3D11Device *dev, float radius, const std::wstring &modelPath) {
        modelOwned = std::make_unique<Model>();
        ModelLoader::LoadFBX(dev, modelPath, *modelOwned);
        modelRef = modelOwned.get();
        collider = MakeSphereCollider(radius);
        rb.invMass = 1.0f; // 动态体
    }

private:
    std::unique_ptr<Model> modelOwned;
};
