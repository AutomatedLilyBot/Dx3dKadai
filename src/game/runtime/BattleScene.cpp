#include "BattleScene.hpp"
#include "MenuScene.hpp"
#include "TransitionScene.hpp"
#include "../entity/BillboardEntity.hpp"
#include "../entity/ExplosionEffect.hpp"

#include <corecrt_startup.h>
#include <windows.h>
#include <string>

#include "SceneManager.hpp"

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

    WorldParams params;
    params.gravity = XMFLOAT3{0, -9.8f, 0};
    world_.setParams(params);
    setupPhysicsCallback();

    createField();
    createNodes();
}

void BattleScene::createField() {
    std::wstring cubePath = ExeDirBattleScene() + L"\\asset\\cube.fbx";
    Model *cube = resourceManager_.getModel(cubePath);
    const int size = 16;
    for (int x = 0; x < size; ++x) {
        for (int z = 0; z < size; ++z) {
            auto block = std::make_unique<BlockEntity>();
            block->setId(allocId());
            block->transform.position = XMFLOAT3{(float) x - size / 2.0f, -0.5f, (float) z - size / 2.0f};
            block->transform.scale = {1.0f, 1.0f, 1.0f};
            block->setCollider(MakeObbCollider(XMFLOAT3{0.5f, 0.5f, 0.5f}));
            block->collider()->updateDerived();
            block->responseType = BlockEntity::ResponseType::None;
            if (cube) block->modelRef = cube;
            registerEntity(*block);
            id2ptr_[block->id()] = block.get();
            entities_.push_back(std::move(block));
        }
    }

    // walls - 四周包围
    for (int i = 0; i < size; ++i) {
        for (int layer = 0; layer < 2; ++layer) {
            // 北墙 (Z-)
            {
                auto wall = std::make_unique<BlockEntity>();
                wall->setId(allocId());
                wall->transform.position = XMFLOAT3{(float) i - size / 2.0f, (float) layer + 0.5f, -size / 2.0f};
                wall->setCollider(MakeObbCollider(XMFLOAT3{0.5f, 0.5f, 0.5f}));
                wall->collider()->updateDerived();
                wall->responseType = BlockEntity::ResponseType::DestroyBullet;
                if (cube) wall->modelRef = cube;
                registerEntity(*wall);
                id2ptr_[wall->id()] = wall.get();
                entities_.push_back(std::move(wall));
            }

            // 南墙 (Z+)
            {
                auto wall = std::make_unique<BlockEntity>();
                wall->setId(allocId());
                wall->transform.position = XMFLOAT3{(float) i - size / 2.0f, (float) layer + 0.5f, size / 2.0f - 1.0f};
                wall->setCollider(MakeObbCollider(XMFLOAT3{0.5f, 0.5f, 0.5f}));
                wall->collider()->updateDerived();
                wall->responseType = BlockEntity::ResponseType::DestroyBullet;
                if (cube) wall->modelRef = cube;
                registerEntity(*wall);
                id2ptr_[wall->id()] = wall.get();
                entities_.push_back(std::move(wall));
            }

            // 西墙 (X-)
            {
                auto wall = std::make_unique<BlockEntity>();
                wall->setId(allocId());
                wall->transform.position = XMFLOAT3{-size / 2.0f, (float) layer + 0.5f, (float) i - size / 2.0f};
                wall->setCollider(MakeObbCollider(XMFLOAT3{0.5f, 0.5f, 0.5f}));
                wall->collider()->updateDerived();
                wall->responseType = BlockEntity::ResponseType::DestroyBullet;
                if (cube) wall->modelRef = cube;
                registerEntity(*wall);
                id2ptr_[wall->id()] = wall.get();
                entities_.push_back(std::move(wall));
            }

            // 东墙 (X+)
            {
                auto wall = std::make_unique<BlockEntity>();
                wall->setId(allocId());
                wall->transform.position = XMFLOAT3{size / 2.0f - 1.0f, (float) layer + 0.5f, (float) i - size / 2.0f};
                wall->setCollider(MakeObbCollider(XMFLOAT3{0.5f, 0.5f, 0.5f}));
                wall->collider()->updateDerived();
                wall->responseType = BlockEntity::ResponseType::DestroyBullet;
                if (cube) wall->modelRef = cube;
                registerEntity(*wall);
                id2ptr_[wall->id()] = wall.get();
                entities_.push_back(std::move(wall));
            }
        }
    }
}

void BattleScene::createNodes() {
    std::wstring cylinderPath = ExeDirBattleScene() + L"\\asset\\cylinder.fbx";
    Model *cylinder = resourceManager_.getModel(cylinderPath);

    auto createNodeAt = [&](const XMFLOAT3 &pos, NodeTeam team) {
        auto node = std::make_unique<NodeEntity>();
        node->setId(allocId());
        node->transform.position = pos;
        node->team = team;

        //node->modelBias.scale = XMFLOAT3{0.5f, 0.5f, 0.5f};

        // 本体碰撞体（主碰撞体，用于物理碰撞）
        auto cap = MakeCapsuleCollider(0.5f, 1.0f);
        cap->setDebugEnabled(true);
        cap->setDebugColor(XMFLOAT4(0, 1, 0, 1));
        node->setCollider(std::move(cap)); // ✅ 使用 setCollider 设置主碰撞体

        // 发射检测触发器（前方区域，只检测不产生物理响应）
        auto obb = MakeObbCollider(XMFLOAT3{0.3f, 0.3f, 0.3f});
        obb->setDebugEnabled(true);
        obb->setDebugColor(XMFLOAT4(0, 1, 1, 1));
        obb->setPosition(XMFLOAT3(0, 0, 0.8f)); // 相对节点本体前方2.1单位
        obb->setIsTrigger(true);
        node->addCollider(std::move(obb)); // ✅ 使用 addCollider 添加额外碰撞体

        // 初始化同步
        for (auto &c: node->colliders()) {
            if (c) c->updateDerived();
        }

        if (cylinder) node->modelRef = cylinder;
        registerEntity(*node);
        id2ptr_[node->id()] = node.get();
        entities_.push_back(std::move(node));
    };

    createNodeAt(XMFLOAT3{0, 1, 0}, NodeTeam::Friendly);
    createNodeAt(XMFLOAT3{2, 1, 2}, NodeTeam::Enemy);
    createNodeAt(XMFLOAT3{-2, 1, -2}, NodeTeam::Neutral);
}

void BattleScene::tick(float dt) {
    // 更新相机（处理平滑插值和 Orbit 模式朝向）
    camera_.update(dt);

    // 调用基类 tick（物理→更新→提交）
    Scene::tick(dt);
}

void BattleScene::handleInput(float dt, const void *window) {
    // 更新 InputManager 状态（右键长按/短按检测）
    inputManager_.updateMouseButtons(dt);

    // === 左键点击：选择/取消选择 Node ===
    static bool leftWasPressed = false;
    bool leftPressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);

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
            if (node && node->team == NodeTeam::Friendly) {
                // 选中友方 Node（RTS 模式下保持相机不变）
                selectedNodeId_ = hitEntity;
                inputManager_.selectNode(hitEntity);
                printf("Selected friendly Node %llu\n", hitEntity);
            } else {
                // 点击了其他物体，取消选择
                selectedNodeId_ = 0;
                inputManager_.deselectNode();
                printf("Clicked non-friendly entity, deselecting\n");
            }
        } else {
            // 没有击中任何物体，取消选择
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

                // 计算从 Node 到点击点的方向
                DirectX::XMFLOAT3 nodePos = selectedNode->transform.position;
                printf("[Right-click] Node at (%.2f, %.2f, %.2f)\n",
                       nodePos.x, nodePos.y, nodePos.z);

                DirectX::XMVECTOR dir = DirectX::XMVectorSubtract(
                    DirectX::XMLoadFloat3(&hitPoint),
                    DirectX::XMLoadFloat3(&nodePos)
                );
                dir = DirectX::XMVector3Normalize(dir);

                DirectX::XMFLOAT3 facingDir;
                DirectX::XMStoreFloat3(&facingDir, dir);

                printf("[Right-click] Facing direction: (%.2f, %.2f, %.2f)\n",
                       facingDir.x, facingDir.y, facingDir.z);

                // 更新 Node 的朝向
                selectedNode->facingDirection = facingDir;

                // 更新 Node 的旋转（让 +Z 轴对准 facingDirection）
                float yaw = atan2f(facingDir.x, facingDir.z);
                printf("[Right-click] Yaw angle: %.2f radians (%.2f degrees)\n",
                       yaw, yaw * 180.0f / 3.14159f);

                selectedNode->transform.setRotationEuler(0, yaw, 0);
                selectedNode->startFiring();
            } else {
                printf("[Right-click] Failed to hit ground plane\n");
            }
        }
    }

    // === 右键长按：绕 Node 旋转相机（Orbit 模式的鼠标控制在 main.cpp 中处理）===
    // 这里不需要额外处理，processMouseMove 会在 Orbit 模式下自动生效

    static bool victoryWasPressed = false;
    static bool defeatWasPressed = false;
    bool victoryPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::V);
    bool defeatPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::B);

    if (victoryPressed && !victoryWasPressed && manager_) {
        manager_->transitionTo(std::make_unique<MenuScene>());
    }
    if (defeatPressed && !defeatWasPressed && manager_) {
        manager_->transitionTo(std::make_unique<MenuScene>());
    }

    victoryWasPressed = victoryPressed;
    defeatWasPressed = defeatPressed;
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
        DirectX::XMFLOAT3 indicatorPos = {nodePos.x, 0.1f, nodePos.z}; // 放在地面上方0.1单位

        // 将世界坐标转换为屏幕坐标
        DirectX::XMVECTOR worldPos = DirectX::XMLoadFloat3(&indicatorPos);
        DirectX::XMMATRIX view = camera_.getViewMatrix();
        DirectX::XMMATRIX proj = camera_.getProjectionMatrix(16.0/9);
        DirectX::XMMATRIX viewProj = DirectX::XMMatrixMultiply(view, proj);

        DirectX::XMVECTOR screenPos = DirectX::XMVector3TransformCoord(worldPos, viewProj);
        DirectX::XMFLOAT3 screenPosF;
        DirectX::XMStoreFloat3(&screenPosF, screenPos);

        // NDC 坐标转换为屏幕坐标 (0-1)
        float screenX = (screenPosF.x + 1.0f) * 0.5f;
        float screenY = (1.0f - screenPosF.y) * 0.5f;

        // 检查是否在屏幕内且在相机前方
        if (screenPosF.z < 0.0f || screenPosF.z > 1.0f ||
            screenX < 0.0f || screenX > 1.0f ||
            screenY < 0.0f || screenY > 1.0f) {
            continue; // 不在屏幕内，跳过
        }

        // 计算箭头的朝向角度（基于 Node 的 facingDirection）
        float yaw = atan2f(node->facingDirection.x, node->facingDirection.z);

        // 加载箭头纹理（使用 number_atlas0.png 作为临时箭头图标）
        std::wstring arrowPath = ExeDirBattleScene() + L"\\asset\\number_atlas0 .png";
        ID3D11ShaderResourceView *arrowTexsrv = resourceManager_.getTextureSrv(arrowPath);

        if (arrowTexsrv) {
            // 绘制箭头（居中在 Node 正下方）
            float arrowSize = 0.05f; // 屏幕空间大小（归一化）
            renderer_->drawUiQuad(
                screenX - arrowSize * 0.5f,
                screenY - arrowSize * 0.5f,
                arrowSize,
                arrowSize,
                arrowTexsrv,
                DirectX::XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), // 黄色
                1.0f,
                0.0f,
                0.0f
            );
        }
    }
}