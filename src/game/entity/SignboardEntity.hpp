#pragma once
#include "BillboardEntity.hpp"
#include "../../core/resource/ResourceManager.hpp"

// 立牌实体
// 特点：仅绕Y轴旋转面向相机，永久存在（除非手动销毁）
class SignboardEntity : public BillboardEntity {
public:
    SignboardEntity() {
        orientationType = BillboardType::YAxisBillboard;
        autoDestroy = false;
        lifetime = 0.0f; // 永久存在
    }

    // 初始化函数
    void initialize(ResourceManager *resourceMgr, const std::wstring &texturePath) {
        if (!resourceMgr) return;

        // 加载立牌模型（使用四边形）
        Model *quadModel = resourceMgr->getModel(L"asset/quad.fbx");
        if (quadModel) {
            modelRef = quadModel;
        }

        // 加载指定纹理
        // TODO: 扩展 ResourceManager 支持纹理加载
        // auto* texture = resourceMgr->getTexture(texturePath);

        // 设置材质
        materialData.baseColorFactor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    // 支持UV滚动的立牌（用于动画文字等）
    void enableUVScroll(float speedX, float speedY) {
        uvScrollSpeed.x = speedX;
        uvScrollSpeed.y = speedY;
    }
};