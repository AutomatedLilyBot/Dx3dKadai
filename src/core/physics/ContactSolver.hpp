#pragma once
#pragma execution_character_set("utf-8")

#include <vector>
#include <DirectXMath.h>
#include "Collider.hpp"

// 与 PhysicsWorld 协作的最小联系结构
struct SolverParams {
    int iterations = 8; // 冲量迭代次数
    float slop = 0.01f; // 穿透容差
    float beta = 0.4f; // 位置校正比例
};

// PhysicsWorld 内部用于解算的体状态镜像
struct BodyState {
    DirectX::XMFLOAT3 p{0, 0, 0};
    DirectX::XMFLOAT3 v{0, 0, 0};
    float invMass = 1.0f;
    float restitution = 0.2f;
    float muS = 0.6f;
    float muK = 0.5f;
};

// 单个接触约束（已过滤 trigger）
struct ContactItem {
    int ia = -1; // body index A
    int ib = -1; // body index B
    OverlapResult c; // 接触数据（法线、点、穿透）
    ColliderBase *ca = nullptr; // 可选：用于事件/调试
    ColliderBase *cb = nullptr;
};

class ContactSolver {
public:
    void solve(std::vector<ContactItem> &contacts,
               std::vector<BodyState> &bodies,
               const SolverParams &params);
};
