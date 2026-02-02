#include "BattleScene.hpp"
#include "MenuScene.hpp"
#include "TransitionScene.hpp"
#include "WinScene.hpp"
#include "LoseScene.hpp"
#include "../entity/BillboardEntity.hpp"
#include "../entity/ExplosionEffect.hpp"

#include <corecrt_startup.h>
#include <windows.h>
#include <string>
#include <ctime>

#include "../runtime/SceneManager.hpp"

using namespace DirectX;

static std::wstring ExeDirBattleScene() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring s(buf);
    auto p = s.find_last_of(L"\\/");
    return (p == std::wstring::npos) ? L"." : s.substr(0, p);
}

void BattleScene::init(Renderer *renderer) {
    renderer_ = renderer;
    resourceManager_.setDevice(renderer ? renderer->device() : nullptr);

    // 初始化相机（RTS 风格：固定高度和俯视角）
    camera_.setPosition(XMFLOAT3{0, 10.0f, -10.0f});
    camera_.setLookAt(XMFLOAT3{0, 0, 0});
    camera_.setMode(CameraMode::RTS);

    // 将相机设置到 Renderer
    if (renderer_) {
        renderer_->setCamera(&camera_);
    }

    // 初始化天空盒
    if (renderer_) {
        // 默认使用纯白色天空盒（用于测试和确认天空盒渲染正常）
        // 可以通过修改RGBA值来改变颜色，例如：
        // - 天蓝色: (135, 206, 235, 255)
        // - 浅灰色: (200, 200, 200, 255)
        // - 纯白色: (255, 255, 255, 255)
        // if (renderer_->createSolidColorSkybox(135, 206, 235, 255)) {
        //     wprintf(L"Default solid color skybox created successfully\n");
        // } else {
        //     wprintf(L"Failed to create default skybox\n");
        // }

        // 如果有纹理文件，可以替换为：
        // std::wstring skyboxPath = ExeDirBattleScene() + L"\\asset\\skybox.dds";
        // renderer_->loadSkyboxFromDDS(skyboxPath);
        //
        // 或者使用6个独立的图片文件：
        std::wstring baseDir = ExeDirBattleScene() + L"\\asset\\skybox\\";
        if (renderer_->loadSkyboxCubeMap(
            baseDir + L"right.png",  // +X
            baseDir + L"left.png",   // -X
            baseDir + L"top.png",    // +Y
            baseDir + L"bottom.png", // -Y
            baseDir + L"front.png",  // +Z
            baseDir + L"back.png"    // -Z
        )) {
            wprintf(L"Skybox cube map loaded successfully\n");
        } else {
            wprintf(L"Failed to load skybox cube map from: %s\n", baseDir.c_str());
            // 如果加载失败，使用备用纯色天空盒
            renderer_->createSolidColorSkybox(255, 255, 255, 255);
        }
    }

    WorldParams params;
    params.gravity = XMFLOAT3{0, -9.8f, 0};
    world_.setParams(params);
    setupPhysicsCallback();

    createField();
    createNodes();
    createUI();

}

void BattleScene::createField() {
    // 加载不同的模型资源
    std::wstring groundPath = ExeDirBattleScene() + L"\\asset\\floor.fbx"; // TODO: 填入地面模型路径
    std::wstring wallPath = ExeDirBattleScene() + L"\\asset\\edge.fbx"; // TODO: 填入墙壁模型路径
    std::wstring cornerPath = ExeDirBattleScene() + L"\\asset\\corner.fbx"; // TODO: 填入转角模型路径

    Model *groundModel = resourceManager_.getModel(groundPath);
    Model *wallModel = resourceManager_.getModel(wallPath);
    Model *cornerModel = resourceManager_.getModel(cornerPath);

    const int size = 32;

    // 生成地面
    for (int x = 0; x < size; ++x) {
        for (int z = 0; z < size; ++z) {
            auto block = std::make_unique<BlockEntity>();
            block->setId(allocId());
            block->transform.position = XMFLOAT3{(float) x - size / 2.0f, -0.5f, (float) z - size / 2.0f};
            block->transform.scale = {1.0f, 1.0f, 1.0f};
            block->setCollider(MakeObbCollider(XMFLOAT3{0.5f, 0.5f, 0.5f}));
            block->collider()->updateDerived();
            block->collider()->setIsStatic(true); // 标记为静态以优化物理检测
            block->responseType = BlockEntity::ResponseType::None;
            if (groundModel) block->modelRef = groundModel;
            registerEntity(*block);
            id2ptr_[block->id()] = block.get();
            entities_.push_back(std::move(block));
        }
    }

    // 墙壁 - 四周包围（留空四角）
    for (int i = 0; i < size; ++i) {
        for (int layer = 0; layer < 2; ++layer) {
            // 北墙 (Z-) - 排除两角
            if (i != 0 && i != size - 1) {
                auto wall = std::make_unique<BlockEntity>();
                wall->setId(allocId());
                wall->transform.position = XMFLOAT3{(float) i - size / 2.0f, (float) layer + 0.5f, -size / 2.0f};

                wall->setCollider(MakeObbCollider(XMFLOAT3{0.5f, 0.5f, 0.5f}));
                wall->collider()->updateDerived();
                wall->collider()->setIsStatic(true);
                wall->responseType = BlockEntity::ResponseType::None;
                if (wallModel) wall->modelRef = wallModel;
                registerEntity(*wall);
                id2ptr_[wall->id()] = wall.get();
                entities_.push_back(std::move(wall));
            }

            // 南墙 (Z+) - 排除两角
            if (i != 0 && i != size - 1) {
                auto wall = std::make_unique<BlockEntity>();
                wall->setId(allocId());
                wall->transform.position = XMFLOAT3{(float) i - size / 2.0f, (float) layer + 0.5f, size / 2.0f - 1.0f};
                wall->setCollider(MakeObbCollider(XMFLOAT3{0.5f, 0.5f, 0.5f}));
                wall->collider()->updateDerived();
                wall->collider()->setIsStatic(true);
                wall->responseType = BlockEntity::ResponseType::None;
                if (wallModel) wall->modelRef = wallModel;
                registerEntity(*wall);
                id2ptr_[wall->id()] = wall.get();
                entities_.push_back(std::move(wall));
            }

            // 西墙 (X-) - 排除两角
            if (i != 0 && i != size - 1) {
                auto wall = std::make_unique<BlockEntity>();
                wall->setId(allocId());
                wall->transform.position = XMFLOAT3{-size / 2.0f, (float) layer + 0.5f, (float) i - size / 2.0f};
                wall->transform.setRotationEuler(0, XM_PIDIV2, 0);
                wall->setCollider(MakeObbCollider(XMFLOAT3{0.5f, 0.5f, 0.5f}));
                wall->collider()->updateDerived();
                wall->collider()->setIsStatic(true);
                wall->responseType = BlockEntity::ResponseType::None;
                if (wallModel) wall->modelRef = wallModel;
                registerEntity(*wall);
                id2ptr_[wall->id()] = wall.get();
                entities_.push_back(std::move(wall));
            }

            // 东墙 (X+) - 排除两角
            if (i != 0 && i != size - 1) {
                auto wall = std::make_unique<BlockEntity>();
                wall->setId(allocId());
                wall->transform.position = XMFLOAT3{size / 2.0f - 1.0f, (float) layer + 0.5f, (float) i - size / 2.0f};
                wall->transform.setRotationEuler(0, XM_PIDIV2, 0);
                wall->setCollider(MakeObbCollider(XMFLOAT3{0.5f, 0.5f, 0.5f}));
                wall->collider()->updateDerived();
                wall->collider()->setIsStatic(true);
                wall->responseType = BlockEntity::ResponseType::None;
                if (wallModel) wall->modelRef = wallModel;
                registerEntity(*wall);
                id2ptr_[wall->id()] = wall.get();
                entities_.push_back(std::move(wall));
            }
        }
    }

    // 四个转角方块 - 单独生成
    const float cornerPositions[4][2] = {
        {-size / 2.0f, -size / 2.0f}, // 西北角
        {size / 2.0f - 1.0f, -size / 2.0f}, // 东北角
        {size / 2.0f - 1.0f, size / 2.0f - 1.0f}, // 东南角
        {-size / 2.0f, size / 2.0f - 1.0f}, // 西南角

    };
    const float cornerRatations[4] = {XM_PI, XM_PIDIV2, XM_PI, XM_PIDIV2};

    for (int layer = 0; layer < 2; ++layer) {
        for (int c = 0; c < 4; ++c) {
            auto corner = std::make_unique<BlockEntity>();
            corner->setId(allocId());
            corner->transform.position = XMFLOAT3{cornerPositions[c][0], (float) layer + 0.5f, cornerPositions[c][1]};
            corner->transform.setRotationEuler(0, cornerRatations[c], 0);
            corner->setCollider(MakeObbCollider(XMFLOAT3{0.5f, 0.5f, 0.5f}));
            corner->collider()->updateDerived();
            corner->collider()->setIsStatic(true);
            corner->responseType = BlockEntity::ResponseType::None;
            if (cornerModel) corner->modelRef = cornerModel;
            registerEntity(*corner);
            id2ptr_[corner->id()] = corner.get();
            entities_.push_back(std::move(corner));
        }
    }

    // 中央斜坡 - 正方形圈
    std::wstring slopePath = ExeDirBattleScene() + L"\\asset\\slope0.fbx";
    Model *slopeModel = resourceManager_.getModel(slopePath);

    const int slopeSize = size / 4; // 斜坡圈的边长
    const float slopeY = 0.5f; // 斜坡的Y位置

    for (int i = 0; i < slopeSize; ++i) {
        // 北斜坡 (Z-) - 排除两角，坡面朝外（朝北）
        if (i != 0 && i != slopeSize - 1) {
            auto slope = std::make_unique<BlockEntity>();
            slope->setId(allocId());
            slope->transform.position = XMFLOAT3{(float) i - slopeSize / 2.0f, slopeY, -slopeSize / 2.0f};
            slope->transform.setRotationEuler(0.0f, 0, 0.0f); // 向北倾斜45度
            slope->transform.scale = {1.0f, 1.0f, 1.0f};
            // 创建倾斜的扁平OBB碰撞体
            auto obb = MakeObbCollider(XMFLOAT3{0.5f, 0.1f, 0.8f});
            obb->setRotationEuler(XMFLOAT3{-XM_PI / 6.0f, 0.0f, 0.0f});
            slope->setCollider(std::move(obb));
            slope->collider()->updateDerived();
            slope->collider()->setIsStatic(true);
            slope->collider()->setDebugEnabled(true);
            slope->responseType = BlockEntity::ResponseType::None;
            if (slopeModel) slope->modelRef = slopeModel;
            registerEntity(*slope);
            id2ptr_[slope->id()] = slope.get();
            entities_.push_back(std::move(slope));
        }

        // 南斜坡 (Z+) - 排除两角，坡面朝外（朝南）
        if (i != 0 && i != slopeSize - 1) {
            auto slope = std::make_unique<BlockEntity>();
            slope->setId(allocId());
            slope->transform.position = XMFLOAT3{(float) i - slopeSize / 2.0f, slopeY, slopeSize / 2.0f - 1.0f};
            slope->transform.setRotationEuler(0.0f, XM_PI, 0.0f); // 向南倾斜45度
            slope->transform.scale = {1.0f, 1.0f, 1.0f};
            auto obb = MakeObbCollider(XMFLOAT3{0.5f, 0.1f, 0.8f});
            obb->setRotationEuler(XMFLOAT3{-XM_PI / 6.0f, 0.0f, 0.0f});
            slope->setCollider(std::move(obb));
            slope->collider()->updateDerived();
            slope->collider()->setIsStatic(true);
            slope->collider()->setDebugEnabled(true);
            slope->responseType = BlockEntity::ResponseType::None;
            if (slopeModel) slope->modelRef = slopeModel;
            registerEntity(*slope);
            id2ptr_[slope->id()] = slope.get();
            entities_.push_back(std::move(slope));
        }

        // 西斜坡 (X-) - 排除两角，坡面朝外（朝西）
        if (i != 0 && i != slopeSize - 1) {
            auto slope = std::make_unique<BlockEntity>();
            slope->setId(allocId());
            slope->transform.position = XMFLOAT3{-slopeSize / 2.0f, slopeY, (float) i - slopeSize / 2.0f};
            slope->transform.setRotationEuler(0.0f, XM_PIDIV2, 0.0f); // 向西倾斜45度
            slope->transform.scale = {1.0f, 1.0f, 1.0f};
            auto obb = MakeObbCollider(XMFLOAT3{0.5f, 0.1f, 0.8f});
            obb->setRotationEuler(XMFLOAT3{-XM_PI / 6.0f, 0.0f, 0.0f});
            slope->setCollider(std::move(obb));
            slope->collider()->updateDerived();
            slope->collider()->setIsStatic(true);
            slope->collider()->setDebugEnabled(true);
            slope->responseType = BlockEntity::ResponseType::None;
            if (slopeModel) slope->modelRef = slopeModel;
            registerEntity(*slope);
            id2ptr_[slope->id()] = slope.get();
            entities_.push_back(std::move(slope));
        }

        // 东斜坡 (X+) - 排除两角，坡面朝外（朝东）
        if (i != 0 && i != slopeSize - 1) {
            auto slope = std::make_unique<BlockEntity>();
            slope->setId(allocId());
            slope->transform.position = XMFLOAT3{slopeSize / 2.0f - 1.0f, slopeY, (float) i - slopeSize / 2.0f};
            slope->transform.setRotationEuler(0.0f, -XM_PIDIV2, 0.0f);; // 向东倾斜45度
            slope->transform.scale = {1.0f, 1.0f, 1.0f};
            auto obb = MakeObbCollider(XMFLOAT3{0.5f, 0.1f, 0.8f});
            obb->setRotationEuler(XMFLOAT3{-XM_PI / 6.0f, 0.0f, 0.0f});
            slope->setCollider(std::move(obb));
            slope->collider()->updateDerived();
            slope->collider()->setIsStatic(true);
            slope->collider()->setDebugEnabled(true);
            slope->responseType = BlockEntity::ResponseType::None;
            if (slopeModel) slope->modelRef = slopeModel;
            registerEntity(*slope);
            id2ptr_[slope->id()] = slope.get();
            entities_.push_back(std::move(slope));
        }
    }
}

void BattleScene::createNodes() {
    std::wstring cylinderPath = ExeDirBattleScene() + L"\\asset\\cylinder.fbx";
    Model *cylinder = resourceManager_.getModel(cylinderPath);

    // 配置参数
    const int totalNodeCount = 16;
    const int initialFriendlyCount = 2;
    const int initialEnemyCount = 1;

    // Field 范围（根据 createField 中的 size=32）
    const float fieldSize = 32.0f;
    const float minX = -fieldSize / 2.0f + 2.0f; // 留2格边距避免太靠近墙壁
    const float maxX = fieldSize / 2.0f - 2.0f;
    const float minZ = -fieldSize / 2.0f + 2.0f;
    const float maxZ = fieldSize / 2.0f - 2.0f;
    const float nodeY = 1.0f;

    // 斜坡区域参数（与 createField 中一致）
    const int slopeSize = static_cast<int>(fieldSize) / 4;
    const float slopeMinX = -slopeSize / 2.0f - 2.0f; // 斜坡区域向外扩展2格
    const float slopeMaxX = slopeSize / 2.0f + 1.0f;
    const float slopeMinZ = -slopeSize / 2.0f - 2.0f;
    const float slopeMaxZ = slopeSize / 2.0f + 1.0f;

    const float minNodeDistance = 2.0f; // 节点之间的最小距离

    // 已生成节点的位置列表
    std::vector<XMFLOAT3> nodePositions;

    // 检查位置是否在斜坡禁区内
    auto isInSlopeZone = [&](float x, float z) {
        return x >= slopeMinX && x <= slopeMaxX && z >= slopeMinZ && z <= slopeMaxZ;
    };

    // 检查与已有节点的最小距离
    auto isTooCloseToOthers = [&](float x, float z) {
        for (const auto &existingPos: nodePositions) {
            float dx = x - existingPos.x;
            float dz = z - existingPos.z;
            float dist = std::sqrt(dx * dx + dz * dz);
            if (dist < minNodeDistance) {
                return true;
            }
        }
        return false;
    };

    // 创建节点的 lambda
    auto createNodeAt = [&](const XMFLOAT3 &pos, NodeTeam team) {
        auto node = std::make_unique<NodeEntity>();
        node->setId(allocId());
        node->transform.position = pos;
        node->setteam(team);

        // 本体碰撞体（主碰撞体，用于物理碰撞）
        auto cap = MakeCapsuleCollider(0.5f, 1.0f);
        cap->setDebugEnabled(false);
        cap->setDebugColor(XMFLOAT4(0, 1, 0, 1));
        node->setCollider(std::move(cap));

        // 初始化同步
        for (auto &c: node->colliders()) {
            if (c) c->updateDerived();
        }

        if (cylinder) node->modelRef = cylinder;
        registerEntity(*node);
        id2ptr_[node->id()] = node.get();
        entities_.push_back(std::move(node));
    };

    // 初始化随机数生成器
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    // 创建队伍分配数组
    std::vector<NodeTeam> teamAssignments;
    for (int i = 0; i < initialFriendlyCount; ++i) {
        teamAssignments.push_back(NodeTeam::Friendly);
    }
    for (int i = 0; i < initialEnemyCount; ++i) {
        teamAssignments.push_back(NodeTeam::Enemy);
    }
    for (int i = initialFriendlyCount + initialEnemyCount; i < totalNodeCount; ++i) {
        teamAssignments.push_back(NodeTeam::Neutral);
    }

    // 打乱队伍分配
    for (int i = teamAssignments.size() - 1; i > 0; --i) {
        int j = std::rand() % (i + 1);
        std::swap(teamAssignments[i], teamAssignments[j]);
    }

    // 生成随机位置并创建节点（带碰撞检测和区域排除）
    for (int i = 0; i < totalNodeCount; ++i) {
        XMFLOAT3 pos;
        int attempts = 0;
        const int maxAttempts = 100; // 防止无限循环

        // 尝试找到合法位置
        do {
            float x = minX + static_cast<float>(std::rand()) / RAND_MAX * (maxX - minX);
            float z = minZ + static_cast<float>(std::rand()) / RAND_MAX * (maxZ - minZ);
            pos = XMFLOAT3{x, nodeY, z};
            attempts++;
        } while ((isInSlopeZone(pos.x, pos.z) || isTooCloseToOthers(pos.x, pos.z)) && attempts < maxAttempts);

        // 如果找到合法位置，创建节点并记录位置
        if (attempts < maxAttempts) {
            createNodeAt(pos, teamAssignments[i]);
            nodePositions.push_back(pos);
        }
    }
}

void BattleScene::createUI() {

    std::wstring scorepath = ExeDirBattleScene() + L"\\asset\\main_scoreboard.png";
    Texture *score= resourceManager_.getTexture(scorepath);

    auto scoreImage = std::make_unique<UIImage>();
    scoreImage->transform().x = 0.4f;
    scoreImage->transform().y = 0.0f;
    scoreImage->transform().width = 0.2f;
    scoreImage->transform().height = scoreImage->transform().width/2;
    scoreImage->setTexture(score);
    scoreImage->setLayer(0);
    addUIElement(std::move(scoreImage));

    std::wstring detailpath = ExeDirBattleScene() + L"\\asset\\main_detail.png";
    Texture *detail= resourceManager_.getTexture(detailpath);

    auto detailImage = std::make_unique<UIImage>();
    detailImage->transform().x = 0.3f;
    detailImage->transform().y = 0.9f;
    detailImage->transform().width = 0.4f;
    detailImage->transform().height = detailImage->transform().width/4;
    detailImage->setTexture(detail);
    detailImage->setLayer(0);
    addUIElement(std::move(detailImage));

    std::wstring numberpath = ExeDirBattleScene() + L"\\asset\\number_atlas1.png";
    Texture *numbertex= resourceManager_.getTexture(numberpath);

    auto totalnum = std::make_unique<UINumberDisplay>();
    totalnum->transform().x = 0.5f;
    totalnum->transform().y = 0.025f;
    totalnum->transform().width = 0.05f;
    totalnum->setDigitSpacing(0);
    totalnum->transform().height = totalnum->transform().width;
    totalnum->setTexture(numbertex);
    totalnum->setLayer(1);
    totalnum->setValue(10);
    totalNodeCount_=totalnum.get();
    addUIElement(std::move(totalnum));

    auto friendlynum = std::make_unique<UINumberDisplay>();
    friendlynum->transform().x = 0.45f;
    friendlynum->transform().y = 0.02f;
    friendlynum->transform().width = 0.05f;
    friendlynum->setDigitSpacing(0);
    friendlynum->transform().height = friendlynum->transform().width;
    friendlynum->setTexture(numbertex);
    friendlynum->setLayer(1);
    friendlynum->setValue(10);
    friendlynum->setTint(XMFLOAT4(0, 0, 1, 1));
    friendlyNodeCount_=friendlynum.get();
    addUIElement(std::move(friendlynum));

    auto enemynum = std::make_unique<UINumberDisplay>();
    enemynum->transform().x = 0.55f;
    enemynum->transform().y = 0.02f;
    enemynum->transform().width = 0.05f;
    enemynum->setDigitSpacing(0);
    enemynum->transform().height = enemynum->transform().width;
    enemynum->setTexture(numbertex);
    enemynum->setLayer(1);
    enemynum->setValue(10);
    enemynum->setTint(XMFLOAT4(1, 0, 0, 1));
    enemyNodeCount_=enemynum.get();
    addUIElement(std::move(enemynum));

    auto healthnum = std::make_unique<UINumberDisplay>();
    healthnum->transform().x = 0.5f;
    healthnum->transform().y = 0.9f;
    healthnum->transform().width = 0.1f;
    healthnum->transform().height = healthnum->transform().width;
    healthnum->setTexture(numbertex);
    healthnum->setLayer(1);
    healthnum->setValue(10);
    healthnum->setTint(XMFLOAT4(0, 1, 1, 1));
    selectedNodeHealth_=healthnum.get();
    addUIElement(std::move(healthnum));

    auto powernum = std::make_unique<UINumberDisplay>();
    powernum->transform().x = 0.4f;
    powernum->transform().y = 0.94f;
    powernum->transform().width = 0.05f;
    powernum->transform().height = powernum->transform().width;
    powernum->setTexture(numbertex);
    powernum->setLayer(1);
    powernum->setValue(9);
    powernum->setTint(XMFLOAT4(0, 0, 1.0, 1));
    selectedNodePower_=powernum.get();
    addUIElement(std::move(powernum));

    auto firenum = std::make_unique<UINumberDisplay>();
    firenum->transform().x = 0.6f;
    firenum->transform().y = 0.94f;
    firenum->transform().width = 0.05f;
    firenum->transform().height = firenum->transform().width;
    firenum->setTexture(numbertex);
    firenum->setLayer(1);
    firenum->setValue(0.3);
    firenum->setTint(XMFLOAT4(0, 1, 0.2, 1));
    selectedNodeFireInterval_=firenum.get();
    addUIElement(std::move(firenum));


}

void BattleScene::tick(float dt) {
    // 更新相机（处理平滑插值和 Orbit 模式朝向）
    camera_.update(dt);

    // 调用基类 tick（物理→更新→提交）
    Scene::tick(dt);

    // 统计 node 数量
    int totalNodes = 0;
    int friendlyNodes = 0;
    int enemyNodes = 0;

    for (auto &entity : entities_) {
        NodeEntity *node = dynamic_cast<NodeEntity *>(entity.get());
        if (node) {
            totalNodes++;
            if (node->getteam() == NodeTeam::Friendly) {
                friendlyNodes++;
            } else if (node->getteam() == NodeTeam::Enemy) {
                enemyNodes++;
            }
        }
    }

    // 演示模式下不触发胜负判定
    if (!isDemoMode_) {
        if (friendlyNodes == totalNodes) {
            manager_->transitionTo(std::make_unique<WinScene>());
            return;
        }
        if (friendlyNodes == 0) {
            manager_->transitionTo(std::make_unique<LoseScene>());
            return;
        }
    }

    // Ensure only the selected friendly node has outline; clear invalid selection.
    if (selectedNodeId_ != 0) {
        NodeEntity *selectedNode = getNodeEntity(selectedNodeId_);
        if (!selectedNode || selectedNode->getteam() != NodeTeam::Friendly) {
            if (selectedNode) {
                selectedNode->materialData.needsOutline = false;
            }
            selectedNodeId_ = 0;
            inputManager_.deselectNode();
        }
    }
    for (auto &entity : entities_) {
        NodeEntity *node = dynamic_cast<NodeEntity *>(entity.get());
        if (!node) continue;
        bool shouldOutline = (node->id() == selectedNodeId_ && node->getteam() == NodeTeam::Friendly);
        if (node->materialData.needsOutline != shouldOutline) {
            node->materialData.needsOutline = shouldOutline;
        }
    }

    // 更新上方显示区的数字
    if (totalNodeCount_) totalNodeCount_->setValue(static_cast<float>(totalNodes));
    if (friendlyNodeCount_) friendlyNodeCount_->setValue(static_cast<float>(friendlyNodes));
    if (enemyNodeCount_) enemyNodeCount_->setValue(static_cast<float>(enemyNodes));

    // 更新下方详情区的数字（显示选中 node 的属性）
    if (selectedNodeId_ != 0) {
        NodeEntity *selectedNode = getNodeEntity(selectedNodeId_);
        if (selectedNode) {
            if (selectedNodeHealth_) selectedNodeHealth_->setValue(static_cast<float>(selectedNode->getHealth()));
            if (selectedNodePower_) selectedNodePower_->setValue(static_cast<float>(selectedNode->getFirepower()));
            if (selectedNodeFireInterval_) selectedNodeFireInterval_->setValue(selectedNode->getfireinterval());
        } else {
            // 选中的节点已被销毁
            if (selectedNodeHealth_) selectedNodeHealth_->setNaN();
            if (selectedNodePower_) selectedNodePower_->setNaN();
            if (selectedNodeFireInterval_) selectedNodeFireInterval_->setNaN();
        }
    } else {
        // 没有选中任何节点
        if (selectedNodeHealth_) selectedNodeHealth_->setNaN();
        if (selectedNodePower_) selectedNodePower_->setNaN();
        if (selectedNodeFireInterval_) selectedNodeFireInterval_->setNaN();
    }
}

void BattleScene::handleInput(float dt, const void *window) {
    // 更新 InputManager 状态（右键长按/短按检测）
    inputManager_.updateMouseButtons(dt);

    // === V键：演示模式切换 ===
    static bool vWasPressed = false;
    bool vPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::V);
    if (vPressed && !vWasPressed) {
        if (isDemoMode_) {
            // 退出演示模式，重置场景
            exitDemoMode();
        } else {
            // 进入演示模式
            enterDemoMode();
        }
    }
    vWasPressed = vPressed;

    // === ESC键：返回主菜单 ===
    static bool escWasPressed = false;
    bool escPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape);
    if (escPressed && !escWasPressed) {
        if (manager_) {
            manager_->transitionTo(std::make_unique<MenuScene>());
        }
    }
    escWasPressed = escPressed;

    // === 左键点击：选择/取消选择 Node ===
    static bool leftWasPressed = false;
    bool leftPressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
    bool hPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::H);

    // 检测按键从未按下到按下的边缘（只在按下瞬间触发一次）
    if (leftPressed && !leftWasPressed) {
        // 获取鼠标窗口坐标
        const sf::Window *sfWindow = static_cast<const sf::Window *>(window);
        sf::Vector2i mousePos = sf::Mouse::getPosition(*sfWindow);

        printf("Mouse clicked at screen position: (%d, %d)\n", mousePos.x, mousePos.y);

        Ray ray = inputManager_.screenPointToRay(mousePos.x, mousePos.y, camera_);
        printf("Ray origin: (%.2f, %.2f, %.2f), dir: (%.2f, %.2f, %.2f)\n",
               ray.origin.x, ray.origin.y, ray.origin.z,
               ray.dir.x, ray.dir.y, ray.dir.z);

        // 射线检测场景中的实体
        EntityId hitEntity = inputManager_.raycastEntities(ray, *this, 100.0f);

        printf("Raycast result: hitEntity = %llu\n", hitEntity);

        if (hitEntity != 0) {
            // 检查是否是 Node
            NodeEntity *node = getNodeEntity(hitEntity);
            if (node && node->getteam() == NodeTeam::Friendly) {
                // 清除旧选中对象的描边标志
                if (selectedNodeId_ != 0) {
                    NodeEntity *oldNode = getNodeEntity(selectedNodeId_);
                    if (oldNode) {
                        oldNode->materialData.needsOutline = false;
                    }
                }

                // 选中友方 Node（RTS 模式下保持相机不变）
                selectedNodeId_ = hitEntity;
                inputManager_.selectNode(hitEntity);
                node->materialData.needsOutline = true; // 设置描边标志
                printf("Selected friendly Node %llu\n", hitEntity);
            } else {
                // 点击了其他物体，取消选择
                if (selectedNodeId_ != 0) {
                    NodeEntity *oldNode = getNodeEntity(selectedNodeId_);
                    if (oldNode) {
                        oldNode->materialData.needsOutline = false;
                    }
                }
                selectedNodeId_ = 0;
                inputManager_.deselectNode();
                printf("Clicked non-friendly entity, deselecting\n");
            }
        } else {
            // 没有击中任何物体，取消选择
            if (selectedNodeId_ != 0) {
                NodeEntity *oldNode = getNodeEntity(selectedNodeId_);
                if (oldNode) {
                    oldNode->materialData.needsOutline = false;
                }
            }
            selectedNodeId_ = 0;
            inputManager_.deselectNode();
            printf("Clicked empty space, deselecting\n");
        }
    }

    leftWasPressed = leftPressed;

    // === 右键短按：指定发射方向（仅在选中 Node 时有效）===
    if (inputManager_.isRightClickShort() && selectedNodeId_ != 0) {
        NodeEntity *selectedNode = getNodeEntity(selectedNodeId_);
        if (selectedNode) {
            // 获取鼠标窗口坐标
            const sf::Window *sfWindow = static_cast<const sf::Window *>(window);
            sf::Vector2i mousePos = sf::Mouse::getPosition(*sfWindow);
            Ray ray = inputManager_.screenPointToRay(mousePos.x, mousePos.y, camera_);

            printf("[Right-click] Mouse at (%d, %d), Ray origin: (%.2f, %.2f, %.2f), dir: (%.2f, %.2f, %.2f)\n",
                   mousePos.x, mousePos.y,
                   ray.origin.x, ray.origin.y, ray.origin.z,
                   ray.dir.x, ray.dir.y, ray.dir.z);

            // 与地面平面相交（Y=0）
            DirectX::XMFLOAT3 hitPoint;
            if (inputManager_.raycastPlane(ray, 0.0f, hitPoint)) {
                printf("[Right-click] Hit ground at (%.2f, %.2f, %.2f)\n",
                       hitPoint.x, hitPoint.y, hitPoint.z);

                // 直接传入世界坐标点，让 setFacingDirection 内部计算方向
                selectedNode->setFacingDirection(hitPoint);
                selectedNode->startFiring();
            } else {
                printf("[Right-click] Failed to hit ground plane\n");
            }
        }
    }

    // Press H to halt fire command
    if (hPressed && selectedNodeId()!=0) {
        NodeEntity *selectedNode = getNodeEntity(selectedNodeId_);
        if (selectedNode) {
            selectedNode->resetfiretimer();
            selectedNode->stopFiring();
        }
    }

    // === 右键长按：绕 Node 旋转相机（Orbit 模式的鼠标控制在 main.cpp 中处理）===
    // 这里不需要额外处理，processMouseMove 会在 Orbit 模式下自动生效

}

NodeEntity *BattleScene::getNodeEntity(EntityId id) {
    auto it = id2ptr_.find(id);
    if (it != id2ptr_.end() && it->second) {
        return dynamic_cast<NodeEntity *>(it->second);
    }
    return nullptr;
}

// 判断实体是否是 Billboard
bool BattleScene::isBillboard(IEntity *entity) const {
    return dynamic_cast<BillboardEntity *>(entity) != nullptr;
}

// 更新 Billboard 朝向
void BattleScene::updateBillboardOrientation(IEntity *entity, const Camera *camera) {
    if (auto *billboard = dynamic_cast<BillboardEntity *>(entity)) {
        billboard->updateBillboardOrientation(camera);
    }
}

// 重写 render 方法以绘制 Node 指示箭头
void BattleScene::render() {
    // 先调用基类的 render 方法绘制所有实体
    Scene::render();

    if (!renderer_) return;

    // 设置透明渲染状态（与 Billboard 相同）
    renderer_->setAlphaBlending(true);
    renderer_->setDepthWrite(false);
    renderer_->setBackfaceCulling(false);

    // 遍历所有实体，为需要显示指示箭头的 Node 绘制箭头
    for (auto &ptr : entities_) {
        if (!ptr) continue;

        // 检查是否是 NodeEntity
        NodeEntity *node = dynamic_cast<NodeEntity *>(ptr.get());
        if (!node || !node->shouldShowDirectionIndicator()) {
            continue;
        }

        // 计算 Node 在世界空间中的位置（正下方）
        DirectX::XMFLOAT3 nodePos = node->transform.position;

        // 计算箭头的朝向角度（基于 Node 的 facingDirection）
        float yaw = atan2f(node->getFacingDirection().x, node->getFacingDirection().z);

        // 构建箭头的变换矩阵
        DirectX::XMMATRIX world =
                    DirectX::XMMatrixScaling(2.0f, 2.0f, 2.0f) *
                    DirectX::XMMatrixRotationY(yaw)*
                    DirectX::XMMatrixTranslation(
                        node->transform.position.x,
                        node->transform.position.y + 0.05f,
                        node->transform.position.z
                    );

        // 加载箭头纹理并创建 quad 模型
        std::wstring arrowPath = ExeDirBattleScene() + L"\\asset\\indicator.png";
        auto arrowModel = resourceManager_.createGroundQuadModelWithTexture(arrowPath);

        if (arrowModel) {
            // 绘制箭头（使用白色tint以显示原始纹理颜色）
            DirectX::XMFLOAT4 tint(1.0f, 1.0f, 1.0f, 1.0f);
            renderer_->drawModel(*arrowModel, world, &tint);
        }
    }

    // 恢复默认渲染状态
    renderer_->setAlphaBlending(false);
    renderer_->setDepthWrite(true);
    renderer_->setBackfaceCulling(true);
}

void BattleScene::enterDemoMode() {
    isDemoMode_ = true;

    // 收集所有node实体
    std::vector<NodeEntity*> nodes;
    for (auto &entity : entities_) {
        NodeEntity *node = dynamic_cast<NodeEntity *>(entity.get());
        if (node) {
            nodes.push_back(node);
        }
    }

    // 随机分配队伍 - 一半friendly，一半enemy
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    std::vector<int> indices;
    for (size_t i = 0; i < nodes.size(); ++i) {
        indices.push_back(static_cast<int>(i));
    }
    // 打乱顺序
    for (size_t i = indices.size() - 1; i > 0; --i) {
        size_t j = std::rand() % (i + 1);
        std::swap(indices[i], indices[j]);
    }

    // 前一半设为friendly，后一半设为enemy
    size_t halfCount = nodes.size() / 2;
    for (size_t i = 0; i < nodes.size(); ++i) {
        NodeEntity *node = nodes[indices[i]];

        // 设置演示模式
        node->setDemoMode(true);

        // 分配队伍
        if (i < halfCount) {
            node->setteam(NodeTeam::Friendly);
        } else {
            node->setteam(NodeTeam::Enemy);
        }
    }

    printf("Entered demo mode with %zu nodes\n", nodes.size());
}

void BattleScene::exitDemoMode() {
    // 通过TransitionScene重置场景
    if (manager_) {
        manager_->transitionTo(std::make_unique<BattleScene>());
    }
}
