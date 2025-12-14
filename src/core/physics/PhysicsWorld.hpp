#pragma once
#pragma execution_character_set("utf-8")

#include <vector>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <cstdint>
#include <DirectXMath.h>

#include "RigidBody.hpp"
#include "Collider.hpp"
#include "ContactSolver.hpp"

// 简易实体标识（游戏层自行保证唯一性/稳定性）
using EntityId = uint32_t;

// 物理世界参数（运行期可调）
struct WorldParams {
    DirectX::XMFLOAT3 gravity{0, -9.8f, 0};
    float maxSpeed = 100.0f; // 可选速度上限
    int substeps = 3; // 子步数量（>1 可减少穿透）
    SolverParams solver; // 解算器参数（iterations/slop/beta）
};

// 触发器事件类型
enum class TriggerPhase { Enter, Stay, Exit };

// 触发事件回调签名
using TriggerCallback = std::function<void(EntityId a, EntityId b, TriggerPhase phase, const OverlapResult & contact)>;

// PhysicsWorld：
// - 负责一帧内的编排（积分→检测→解算→同步/事件）
// - 使用索引化稠密数组提升解算效率；通过映射维护 collider/entity 关系
class PhysicsWorld {
public:
    PhysicsWorld() = default;

    void setParams(const WorldParams &p);

    const WorldParams &params() const { return params_; }

    // 注册/反注册：一个实体可绑定一个 RigidBody 与若干 Collider
    void registerEntity(EntityId e, RigidBody *rb, std::span<ColliderBase *> colliders);

    void unregisterEntity(EntityId e);

    // 主更新入口
    void step(float dt);

    // 触发器事件回调（可选）
    void setTriggerCallback(TriggerCallback cb) { onTrigger_ = std::move(cb); }

    // —— 新增：从外部 Owner Transform 同步到物理体 ——
    // 用途：当游戏层直接改动实体 Transform（传送/拖拽/重置）时，在物理步开始前调用该接口，
    // 若位置与当前 BodyState 不一致，则以 Owner 的位置为准覆盖，并可选清零速度（避免残余动量）。
    // 说明：当前解算仅支持线性部分，rotEulerW 仅用于将 Owner 世界朝向注入到 Collider（影响派生世界矩阵/调试绘制），
    // 不参与动力学计算。
    void syncOwnerTransform(EntityId e,
                            const DirectX::XMFLOAT3 &posW,
                            const DirectX::XMFLOAT3 &rotEulerW,
                            bool resetVelocityOnChange = true);

private:
    // 内部过程
    void integrate(float dt);

    void broadPhase();

    void narrowPhase();

    void solveContacts();

    void positionalCorrection(); // 若解算器已做，可为空实现
    void syncBackAndDispatch(float dt);

    // 在进行广相/窄相前，将 BodyState 的位置写回到对应的 Collider，刷新 AABB
    void syncBodiesToColliders();

    // 工具
    static uint64_t PairKey(EntityId a, EntityId b);

private:
    // 稠密体数组（镜像数据，解算直接操作）
    std::vector<BodyState> bodies_; // 索引 → 线性状态
    std::vector<RigidBody *> bodyRefs_; // 索引 → 原始刚体指针（写回）
    std::vector<std::vector<ColliderBase *> > collidersByBody_; // 每个体绑定的 colliders

    // Collider 与实体/体索引映射
    std::vector<ColliderBase *> colliders_; // 所有注册的 collider（供广相）
    std::unordered_map<ColliderBase *, int> col2bodyIdx_;
    std::unordered_map<ColliderBase *, EntityId> col2entity_;

    // 广相/窄相临时
    std::vector<ColliderPair> pairs_;
    std::vector<ContactItem> contacts_;
    std::unordered_map<uint64_t, OverlapResult> triggerContactMap_; // 本帧触发对 -> 联系触点

    // 触发器状态（帧间对比）
    std::unordered_set<uint64_t> lastTriggers_;
    std::unordered_set<uint64_t> currTriggers_;

    // 事件回调
    TriggerCallback onTrigger_{};

    // 组件
    ContactSolver solver_;
    WorldParams params_{};

    // 便捷映射：实体 → 刚体索引、实体 → 其 colliders
    std::unordered_map<EntityId, int> entity2bodyIdx_;
    std::unordered_map<EntityId, std::vector<ColliderBase *> > entity2colliders_;
};