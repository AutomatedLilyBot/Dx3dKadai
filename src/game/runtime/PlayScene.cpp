#include "PlayScene.hpp"
#include "game/entity/StaticEntity.hpp"
#include "game/entity/SpawnerEntity.hpp"
#include "core/gfx/ModelLoader.hpp"
#include <windows.h>
#include <string>

using namespace DirectX;

static std::wstring ExeDirPlayScene() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring s(buf);
    auto p = s.find_last_of(L"\\/");
    return (p == std::wstring::npos) ? L"." : s.substr(0, p);
}

void PlayScene::init(Renderer *renderer) {
    renderer_ = renderer;

    // 设置物理参数
    WorldParams params;
    params.gravity = XMFLOAT3{0, -1.0f, 0};
    world_.setParams(params);

    // 安装物理回调
    setupPhysicsCallback();

    // 加载平台模型
    std::wstring cubePath = ExeDirPlayScene() + L"\\asset\\cube.fbx";
    platformModelLoaded_ = ModelLoader::LoadFBX(renderer_->device(), cubePath, platformModel_);

    // 创建平台（静态立方体）
    {
        auto plat = std::make_unique<StaticEntity>();
        plat->setId(allocId());

        if (platformModelLoaded_) {
            plat->modelRef = &platformModel_;
        }

        plat->transform.scale = {10.0f, 10.0f, 1.0f};
        plat->transform.position = {0.0f, -2.0f, 0.0f};
        plat->transform.setRotationEuler(XM_PIDIV2, 0, XM_PIDIV2 * 0.2f);

        // 物理：OBB 半尺寸与缩放相匹配
        plat->collider = MakeObbCollider(XMFLOAT3{10.0f, 10.0, 1.0f});
        plat->collider->setDebugEnabled(true);
        plat->collider->setDebugColor(XMFLOAT4(1, 0, 0, 1));

        // 注册
        registerEntity(*plat);
        id2ptr_[plat->id()] = plat.get();
        entities_.push_back(std::move(plat));
    }

    // 创建生成器（触发器 OBB，无模型）
    {
        auto sp = std::make_unique<SpawnerEntity>();
        sp->setId(allocId());
        sp->modelRef = nullptr; // 不渲染
        sp->transform.position = {0.0f, 3.0f, 0.0f};
        sp->transform.scale = {1.0f, 1.0f, 1.0f};
        sp->collider = MakeObbCollider(XMFLOAT3{0.5f, 0.5f, 0.5f});
        sp->collider->setIsTrigger(true);
        sp->spawnInterval = 0.15f;
        sp->ballRadius = 0.25f;
        sp->spawnYOffset = -0.5f; // 调整偏移，让球生成在 trigger 内部

        registerEntity(*sp);
        id2ptr_[sp->id()] = sp.get();
        entities_.push_back(std::move(sp));
    }
}
