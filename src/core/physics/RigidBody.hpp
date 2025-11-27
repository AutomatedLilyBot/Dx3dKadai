#pragma once
#pragma execution_character_set("utf-8")

#include <DirectXMath.h>

// 轻量级刚体组件（首版：仅线性部分，无旋转）
// 说明：
// - 该结构由游戏层（Entity）持有，PhysicsWorld 在解算时会读取/写回其 position/velocity。
// - 如果 invMass == 0 则视为静态体（不受力，不移动）。
struct RigidBody {
    DirectX::XMFLOAT3 position{0, 0, 0};
    DirectX::XMFLOAT3 velocity{0, 0, 0};

    // 逆质量：0 表示静态
    float invMass = 1.0f;

    // 材质参数
    float restitution = 0.2f; // 恢复系数 e ∈ [0,1]
    float muS = 0.6f; // 静摩擦（预留，首版未区分）
    float muK = 0.5f; // 动摩擦

    // 注册索引：由 PhysicsWorld 赋值（-1 表示未注册）
    int bodyIdx = -1;
};
