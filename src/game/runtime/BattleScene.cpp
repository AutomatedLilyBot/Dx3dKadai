#include "BattleScene.hpp"
#include <windows.h>
#include <string>

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
            block->transform.position = XMFLOAT3{(float)x - size / 2.0f, -1.0f, (float)z - size / 2.0f};
            block->transform.scale = {1.0f, 1.0f, 1.0f};
            block->setCollider(MakeObbCollider(XMFLOAT3{0.5f, 0.5f, 0.5f}));
            block->collider()->updateDerived();
            block->responseType = BlockEntity::ResponseType::BounceBullet;
            if (cube) block->modelRef = cube;
            registerEntity(*block);
            id2ptr_[block->id()] = block.get();
            entities_.push_back(std::move(block));
        }
    }

    // walls - create complete perimeter
    // negative z edge
    for (int i = 0; i < size; ++i) {
        for (int layer = 0; layer < 2; ++layer) {
            auto wall = std::make_unique<BlockEntity>();
            wall->setId(allocId());
            wall->transform.position = XMFLOAT3{(float)i - size / 2.0f, (float)layer, -size / 2.0f};
            wall->setCollider(MakeObbCollider(XMFLOAT3{0.5f, 0.5f, 0.5f}));
            wall->responseType = BlockEntity::ResponseType::DestroyBullet;
            if (cube) wall->modelRef = cube;
            registerEntity(*wall);
            id2ptr_[wall->id()] = wall.get();
            entities_.push_back(std::move(wall));
        }
    }
    
    // positive z edge
    for (int i = 0; i < size; ++i) {
        for (int layer = 0; layer < 2; ++layer) {
            auto wall = std::make_unique<BlockEntity>();
            wall->setId(allocId());
            wall->transform.position = XMFLOAT3{(float)i - size / 2.0f, (float)layer, size / 2.0f - 1.0f};
            wall->setCollider(MakeObbCollider(XMFLOAT3{0.5f, 0.5f, 0.5f}));
            wall->responseType = BlockEntity::ResponseType::DestroyBullet;
            if (cube) wall->modelRef = cube;
            registerEntity(*wall);
            id2ptr_[wall->id()] = wall.get();
            entities_.push_back(std::move(wall));
        }
    }
    
    // negative x edge
    for (int i = 0; i < size; ++i) {
        for (int layer = 0; layer < 2; ++layer) {
            auto wall = std::make_unique<BlockEntity>();
            wall->setId(allocId());
            wall->transform.position = XMFLOAT3{-size / 2.0f, (float)layer, (float)i - size / 2.0f};
            wall->setCollider(MakeObbCollider(XMFLOAT3{0.5f, 0.5f, 0.5f}));
            wall->responseType = BlockEntity::ResponseType::DestroyBullet;
            if (cube) wall->modelRef = cube;
            registerEntity(*wall);
            id2ptr_[wall->id()] = wall.get();
            entities_.push_back(std::move(wall));
        }
    }
    
    // positive x edge
    for (int i = 0; i < size; ++i) {
        for (int layer = 0; layer < 2; ++layer) {
            auto wall = std::make_unique<BlockEntity>();
            wall->setId(allocId());
            wall->transform.position = XMFLOAT3{size / 2.0f - 1.0f, (float)layer, (float)i - size / 2.0f};
            wall->setCollider(MakeObbCollider(XMFLOAT3{0.5f, 0.5f, 0.5f}));
            wall->responseType = BlockEntity::ResponseType::DestroyBullet;
            if (cube) wall->modelRef = cube;
            registerEntity(*wall);
            id2ptr_[wall->id()] = wall.get();
            entities_.push_back(std::move(wall));
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
        node->setCollider(MakeCapsuleCollider(XMFLOAT3{0.5f, 1.0f, 0.5f}));
        node->collider()->updateDerived();
        if (cylinder) node->modelRef = cylinder;
        registerEntity(*node);
        id2ptr_[node->id()] = node.get();
        entities_.push_back(std::move(node));
    };

    createNodeAt(XMFLOAT3{0, 0, 0}, NodeTeam::Friendly);
    createNodeAt(XMFLOAT3{2, 0, 2}, NodeTeam::Enemy);
    createNodeAt(XMFLOAT3{-2, 0, -2}, NodeTeam::Neutral);
}
