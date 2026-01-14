#pragma once
#include "BillboardEntity.hpp"
#include "../../core/resource/ResourceManager.hpp"
#include <memory>
#include <windows.h>

// 爆炸特效实体
// 特点：完全朝向相机，短暂生命周期，自动销毁
class ExplosionEffect : public BillboardEntity {
public:
    ExplosionEffect() {
        orientationType = BillboardType::FullBillboard;
        autoDestroy = true;
        lifetime = 0.5f; // 0.5秒后自动消失
        currentTime = 0.0f;
    }

    // 初始化函数（由 spawn lambda 调用）
    void initialize(ResourceManager *resourceMgr) {
        if (!resourceMgr) return;

        // 获取可执行文件目录
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        std::wstring exePath(buf);
        auto lastSlash = exePath.find_last_of(L"\\/");
        std::wstring exeDir = (lastSlash == std::wstring::npos) ? L"." : exePath.substr(0, lastSlash);

        // 尝试创建带纹理的四边形模型
        std::wstring texturePath = exeDir + L"\\asset\\explosion.png";
        ownedModel_ = resourceMgr->createQuadModelWithTexture(texturePath);

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

        // 不需要碰撞体（纯视觉效果）
    }

    // 可选：支持序列帧动画的初始化
    void initializeWithAnimation(ResourceManager *resourceMgr, int frames, float frameDuration) {
        initialize(resourceMgr);
        frameCount = frames;
        frameTime = frameDuration;
        currentFrame = 0;
        frameTimer = 0.0f;
    }

private:
    std::unique_ptr<Model> ownedModel_; // 拥有的独立 Model（包含纹理）
};
