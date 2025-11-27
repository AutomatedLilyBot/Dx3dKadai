#pragma once
#include "StaticEntity.hpp"
#include "../../core/physics/RigidBody.hpp"

// 动态实体：在 StaticEntity 基础上增加 RigidBody，用于物理解算
class DynamicEntity : public StaticEntity {
public:
    RigidBody rb; // invMass==0 表示静态；默认由调用方设置为非零质量

    RigidBody *rigidBody() override { return &rb; }
};
