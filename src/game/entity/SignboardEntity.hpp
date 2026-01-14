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

        // 获取可执行文件目录
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        std::wstring exePath(buf);
        auto lastSlash = exePath.find_last_of(L"\\/");
        std::wstring exeDir = (lastSlash == std::wstring::npos) ? L"." : exePath.substr(0, lastSlash);

        // 尝试创建带纹理的四边形模型
        std::wstring texPath = exeDir + L"\\asset\\explosion.png";
        ownedModel_ = resourceMgr->createQuadModelWithTexture(texPath);

        if (ownedModel_) {
            // 成功创建带纹理的模型
            modelRef = ownedModel_.get();
            // 使用白色基础颜色因子，让纹理完全显示
            materialData.baseColorFactor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 0.8f);
        } else {
            // 创建失败，回退到无纹理的共享模型
            Model *quadModel = resourceMgr->getQuadModel();
            if (quadModel) {
                modelRef = quadModel;
            }
            // 使用橙红色作为纯色特效
            materialData.baseColorFactor = DirectX::XMFLOAT4(1.0f, 0.5f, 0.1f, 0.8f);
        }

    }

    // 支持UV滚动的立牌（用于动画文字等）
    void enableUVScroll(float speedX, float speedY) {
        uvScrollSpeed.x = speedX;
        uvScrollSpeed.y = speedY;
    }
private:
    std::unique_ptr<Model> ownedModel_;
};