#pragma once
#pragma execution_character_set("utf-8")
// 新一代碰撞体接口（仅草案/头文件，暂无实现）
// 说明：
// - 仅支持三类：Sphere、OBB、Capsule。
// - 每个碰撞体自持位移/旋转/缩放，并通过更新接口（返回 bool）强制缩放约束：
//   - Sphere：仅允许均匀缩放（sx==sy==sz）。
//   - Capsule：仅允许沿长轴改变长度；半径方向必须等比缩放。
//   - OBB：允许任意非均匀缩放。
// - 本文件只定义接口与数据结构；算法与实现留待后续阶段。

#include <cstdint>
#include <vector>
#include <utility>
#include <memory>
#include <DirectXMath.h>

// 前置声明，避免图形模块的包含循环
struct Model;

// 基础类型与配置
enum class ColliderType { Sphere, Obb, Capsule };

struct Aabb {
    DirectX::XMFLOAT3 min{0, 0, 0};
    DirectX::XMFLOAT3 max{0, 0, 0};
};

struct OverlapResult {
    bool intersects = false; // 首期仅使用布尔值；后续扩展深度/法线/接触点
    float penetration = 0.0f;
    DirectX::XMFLOAT3 normal{0, 0, 0};
    DirectX::XMFLOAT3 pointOnA{0, 0, 0};
    DirectX::XMFLOAT3 pointOnB{0, 0, 0};
};

// 全局数值精度配置（固定但可调）
struct PhysicsConfig {
    float epsilon = 1e-5f;
};

// 获取/设置全局 PhysicsConfig（仅声明，实现在 .cpp）
const PhysicsConfig &GetPhysicsConfig();

void SetPhysicsEpsilon(float e);

// 基类接口：仅定义契约，不提供存储
class ColliderBase {
public:
    virtual ~ColliderBase() = default;

    // 类型查询
    virtual ColliderType kind() const = 0;

    // 变换：位置/旋转（欧拉）/缩放。违反约束时返回 false 并且不改变内部状态。
    virtual bool setPosition(const DirectX::XMFLOAT3 &pos) = 0;

    virtual bool setRotationEuler(const DirectX::XMFLOAT3 &rotEuler) = 0; // pitch, yaw, roll（弧度）
    virtual bool setScale(const DirectX::XMFLOAT3 &scale) = 0;

    // 读取当前 TRS
    virtual DirectX::XMFLOAT3 position() const = 0;

    virtual DirectX::XMFLOAT3 rotationEuler() const = 0;

    virtual DirectX::XMFLOAT3 scale() const = 0;

    // 世界矩阵（S*R*T），供渲染/调试；实现方可选择延迟更新或即时构造
    virtual DirectX::XMMATRIX world() const = 0;

    // 派生数据更新（刷新 AABB/世界参数等）。若内部状态非法（如缩放不同步），可返回 false。
    virtual bool updateDerived() = 0;

    // AABB（世界空间，缓存或即时计算均可）
    virtual Aabb aabb() const = 0;

    // DebugDraw 控制（每体独立颜色与开关）
    virtual void setDebugEnabled(bool enabled) = 0;

    virtual bool debugEnabled() const = 0;

    virtual void setDebugColor(const DirectX::XMFLOAT4 &rgba) = 0;

    virtual DirectX::XMFLOAT4 debugColor() const = 0;

    // 绑定偏差（相对于绑定 GameObject 的参考变换点的位移偏差，默认 {0,0,0}）
    // 用途：在一个 GameObject 绑定多个碰撞体时，持久化记录每个碰撞体相对主体的位移偏差，
    //      以防同步/序列化过程中丢失中心差导致无法还原布局。
    // 说明：该偏差不强加到 world() 的计算方式中，由上层（实体/同步器）在需要时自行组合使用。
    virtual bool setOwnerOffset(const DirectX::XMFLOAT3 &offset) = 0;

    virtual DirectX::XMFLOAT3 ownerOffset() const = 0;

    // Trigger 标志：标记为 trigger 的碰撞体仅用于触发事件，不产生物理碰撞响应
    virtual void setIsTrigger(bool trigger) = 0;

    virtual bool isTrigger() const = 0;
};

// Sphere：局部参数为 centerLocal（可选）+ radiusLocal；世界半径=radiusLocal*uniformScale
class SphereCollider : public ColliderBase {
public:
    // 访问器（只读）
    virtual float radiusLocal() const = 0;

    virtual float radiusWorld() const = 0;

    virtual DirectX::XMFLOAT3 centerWorld() const = 0;
};

// OBB：局部参数 center + halfExtents；世界派生：centerW, axesW[3], halfExtentsW
class ObbCollider : public ColliderBase {
public:
    virtual DirectX::XMFLOAT3 centerWorld() const = 0;

    virtual void axesWorld(DirectX::XMFLOAT3 outAxes[3]) const = 0; // 3 个单位轴
    virtual DirectX::XMFLOAT3 halfExtentsWorld() const = 0;
};

// Capsule：局部参数 p0, p1（长轴线段）+ radius；世界派生：p0W, p1W, radiusW
class CapsuleCollider : public ColliderBase {
public:
    virtual std::pair<DirectX::XMFLOAT3, DirectX::XMFLOAT3> segmentWorld() const = 0; // p0W, p1W
    virtual float radiusWorld() const = 0;
};

// 统一检测入口（仅声明，实现在 .cpp）
bool Intersect(const ColliderBase &A, const ColliderBase &B); // 首期布尔相交
bool Intersect(const ColliderBase &A, const ColliderBase &B, OverlapResult &out);

// 批量相交（O(n^2) 验证版）
using ColliderPair = std::pair<ColliderBase *, ColliderBase *>;

size_t overlapAll(const std::vector<ColliderBase *> &colliders, std::vector<ColliderPair> &outPairs);

// 自动拟合接口（草案）
struct FitOptions {
    enum class Mode { WholeModel, PerMesh, AutoBest };

    enum class Preferred { Any, Sphere, Obb, Capsule };

    Mode mode = Mode::WholeModel;
    Preferred preferred = Preferred::Any; // 若非 Any，则仅拟合单一类型并返回一个
    int maxCount = 8; // AutoBest 上限
    float errorThreshold = 0.0f; // 误差阈值（按场景尺度指定）
    float tightness = 0.75f; // 越大越“紧”
    int minPoints = 64; // AutoBest 分块的最小点数
    int maxDepth = 6; // AutoBest 体素分裂最大深度
};

// 根据 Model 和选项生成碰撞体集合（仅声明）
std::vector<ColliderBase *> FitFromModel(const Model &model, const FitOptions &options);

// 简易工厂：创建三类碰撞体的默认实现实例（第一阶段提供最小可用实现）
std::unique_ptr<SphereCollider> MakeSphereCollider(float radiusLocal);

std::unique_ptr<ObbCollider> MakeObbCollider(const DirectX::XMFLOAT3 &halfExtentsLocal);

std::unique_ptr<CapsuleCollider> MakeCapsuleCollider(const DirectX::XMFLOAT3 &p0Local,
                                                     const DirectX::XMFLOAT3 &p1Local,
                                                     float radiusLocal);
