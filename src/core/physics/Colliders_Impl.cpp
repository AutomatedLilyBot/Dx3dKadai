#include "Collider.hpp"
#include "Transform.hpp"
#include <cmath>
#include <algorithm>

using namespace DirectX;

static inline bool NearlyEqual(float a, float b, float eps) {
    return std::fabs(a - b) <= eps;
}

static XMMATRIX MakeWorld(const XMFLOAT3 &pos, const XMFLOAT3 &euler, const XMFLOAT3 &scl) {
    XMMATRIX S = XMMatrixScaling(scl.x, scl.y, scl.z);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(euler.x, euler.y, euler.z);
    XMMATRIX T = XMMatrixTranslation(pos.x, pos.y, pos.z);
    return S * R * T;
}

namespace {
    class SphereColliderImpl final : public SphereCollider {
    public:
        explicit SphereColliderImpl(float rLocal)
            : m_radiusLocal(std::max(0.0f, rLocal)) {
        }

        ColliderType kind() const override { return ColliderType::Sphere; }

        // 局部偏移（相对 Owner）
        bool setPosition(const XMFLOAT3 &pos) override {
            m_localOffset = pos;
            return true;
        }

        bool setRotationEuler(const XMFLOAT3 &rotEuler) override {
            m_localRot = rotEuler;
            return true;
        }

        bool setScale(const XMFLOAT3 &scale) override {
            float eps = GetPhysicsConfig().epsilon;
            if (!NearlyEqual(scale.x, scale.y, eps) || !NearlyEqual(scale.x, scale.z, eps)) return false;
            if (scale.x <= 0 || scale.y <= 0 || scale.z <= 0) return false;
            m_scl = scale;
            return true;
        }

        XMFLOAT3 position() const override { return m_localOffset; }
        XMFLOAT3 rotationEuler() const override { return m_localRot; }
        XMFLOAT3 scale() const override { return m_scl; }

        // 组合 Owner 世界位姿 + 局部偏移/旋转
        XMMATRIX world() const override {
            XMMATRIX S = XMMatrixScaling(m_scl.x, m_scl.y, m_scl.z);
            XMMATRIX Rowner = XMMatrixRotationRollPitchYaw(m_ownerRot.x, m_ownerRot.y, m_ownerRot.z);
            XMMATRIX Rlocal = XMMatrixRotationRollPitchYaw(m_localRot.x, m_localRot.y, m_localRot.z);
            XMMATRIX R = Rlocal * Rowner; // 先局部再跟随 Owner
            // 世界中心 = ownerPos + Rowner * localOffset（Sphere 对旋转不敏感，仅用于偏移）
            XMVECTOR off = XMVectorSet(m_localOffset.x, m_localOffset.y, m_localOffset.z, 0);
            off = XMVector3TransformNormal(off, Rowner);
            XMFLOAT3 centerW{m_ownerPos.x, m_ownerPos.y, m_ownerPos.z};
            XMVECTOR c = XMVectorSet(centerW.x, centerW.y, centerW.z, 1.0f);
            c = XMVectorAdd(c, off);
            XMFLOAT3 cw{};
            XMStoreFloat3(&cw, c);
            XMMATRIX T = XMMatrixTranslation(cw.x, cw.y, cw.z);
            return S * R * T;
        }

        bool updateDerived() override { return true; }

        Aabb aabb() const override {
            XMFLOAT3 c = centerWorld();
            float r = radiusWorld();
            return {XMFLOAT3{c.x - r, c.y - r, c.z - r}, XMFLOAT3{c.x + r, c.y + r, c.z + r}};
        }

        void setDebugEnabled(bool enabled) override { m_dbgEnabled = enabled; }
        bool debugEnabled() const override { return m_dbgEnabled; }
        void setDebugColor(const XMFLOAT4 &rgba) override { m_dbgColor = rgba; }
        XMFLOAT4 debugColor() const override { return m_dbgColor; }

        bool setOwnerOffset(const XMFLOAT3 &offset) override {
            m_ownerOffset = offset;
            return true;
        }

        XMFLOAT3 ownerOffset() const override { return m_ownerOffset; }

        void setIsTrigger(bool trigger) override { m_isTrigger = trigger; }
        bool isTrigger() const override { return m_isTrigger; }

        void setIsStatic(bool isStatic) override { m_isStatic = isStatic; }
        bool isStatic() const override { return m_isStatic; }

        // Sphere specifics
        float radiusLocal() const override { return m_radiusLocal; }
        float radiusWorld() const override { return m_radiusLocal * m_scl.x; }

        XMFLOAT3 centerWorld() const override {
            XMMATRIX Rowner = XMMatrixRotationRollPitchYaw(m_ownerRot.x, m_ownerRot.y, m_ownerRot.z);
            XMVECTOR off = XMVectorSet(m_localOffset.x, m_localOffset.y, m_localOffset.z, 0);
            off = XMVector3TransformNormal(off, Rowner);
            XMVECTOR base = XMVectorSet(m_ownerPos.x, m_ownerPos.y, m_ownerPos.z, 1.0f);
            XMVECTOR c = XMVectorAdd(base, off);
            XMFLOAT3 cw{};
            XMStoreFloat3(&cw, c);
            return cw;
        }

        // Owner 世界位姿注入/读取
        void setOwnerWorldPosition(const XMFLOAT3 &ownerPosW) override { m_ownerPos = ownerPosW; }
        void setOwnerWorldRotationEuler(const XMFLOAT3 &ownerRotEulerW) override { m_ownerRot = ownerRotEulerW; }
        XMFLOAT3 ownerWorldPosition() const override { return m_ownerPos; }
        XMFLOAT3 ownerWorldRotationEuler() const override { return m_ownerRot; }

        // 射线相交检测（球体）
        bool intersectsRay(const XMFLOAT3 &rayOrigin, const XMFLOAT3 &rayDir, float &outDistance) const override {
            XMFLOAT3 center = centerWorld();
            float radius = radiusWorld();

            XMVECTOR oc = XMVectorSet(
                rayOrigin.x - center.x,
                rayOrigin.y - center.y,
                rayOrigin.z - center.z,
                0
            );
            XMVECTOR dir = XMLoadFloat3(&rayDir);

            float a = XMVectorGetX(XMVector3Dot(dir, dir));
            float b = 2.0f * XMVectorGetX(XMVector3Dot(oc, dir));
            float c = XMVectorGetX(XMVector3Dot(oc, oc)) - radius * radius;

            float discriminant = b * b - 4 * a * c;
            if (discriminant < 0) return false;

            float t = (-b - sqrtf(discriminant)) / (2.0f * a);
            if (t < 0) {
                t = (-b + sqrtf(discriminant)) / (2.0f * a);
                if (t < 0) return false;
            }

            outDistance = t;
            return true;
        }

        // 获取世界位置（Owner世界位置 + 旋转后的局部偏移）
        XMFLOAT3 getWorldPosition() const override {
            return centerWorld();  // 球体的世界位置就是中心点
        }

        // 获取世界旋转（Owner世界旋转 + 局部旋转）
        XMFLOAT3 getWorldRotationEuler() const override {
            // 组合旋转：局部旋转 * Owner旋转
            // 对于欧拉角，简单相加（近似，严格应该用四元数）
            return XMFLOAT3{
                m_ownerRot.x + m_localRot.x,
                m_ownerRot.y + m_localRot.y,
                m_ownerRot.z + m_localRot.z
            };
        }

    private:
        // Owner 世界位姿
        XMFLOAT3 m_ownerPos{0, 0, 0};
        XMFLOAT3 m_ownerRot{0, 0, 0};
        // 局部偏移/旋转（相对 Owner）
        XMFLOAT3 m_localOffset{0, 0, 0};
        XMFLOAT3 m_localRot{0, 0, 0};
        XMFLOAT3 m_scl{1, 1, 1};
        float m_radiusLocal{0};
        bool m_dbgEnabled{false};
        XMFLOAT4 m_dbgColor{0, 0.5f, 1, 1};
        XMFLOAT3 m_ownerOffset{0, 0, 0};
        bool m_isTrigger{false};
        bool m_isStatic{false};
    };

    class ObbColliderImpl final : public ObbCollider {
    public:
        explicit ObbColliderImpl(const XMFLOAT3 &heLocal) : m_halfLocal(heLocal) {
        }

        ColliderType kind() const override { return ColliderType::Obb; }

        bool setPosition(const XMFLOAT3 &pos) override {
            m_localOffset = pos;
            return true;
        }

        bool setRotationEuler(const XMFLOAT3 &rotEuler) override {
            m_localRot = rotEuler;
            return true;
        }

        bool setScale(const XMFLOAT3 &scale) override {
            if (scale.x <= 0 || scale.y <= 0 || scale.z <= 0) return false;
            m_scl = scale;
            return true;
        }

        XMFLOAT3 position() const override { return m_localOffset; }
        XMFLOAT3 rotationEuler() const override { return m_localRot; }
        XMFLOAT3 scale() const override { return m_scl; }

        XMMATRIX world() const override {
            XMMATRIX S = XMMatrixScaling(m_scl.x, m_scl.y, m_scl.z);
            XMMATRIX Rowner = XMMatrixRotationRollPitchYaw(m_ownerRot.x, m_ownerRot.y, m_ownerRot.z);
            XMMATRIX Rlocal = XMMatrixRotationRollPitchYaw(m_localRot.x, m_localRot.y, m_localRot.z);
            XMMATRIX R = Rlocal * Rowner;
            XMVECTOR off = XMVectorSet(m_localOffset.x, m_localOffset.y, m_localOffset.z, 0);
            off = XMVector3TransformNormal(off, Rowner);
            XMVECTOR base = XMVectorSet(m_ownerPos.x, m_ownerPos.y, m_ownerPos.z, 1.0f);
            XMVECTOR c = XMVectorAdd(base, off);
            XMFLOAT3 cw{};
            XMStoreFloat3(&cw, c);
            XMMATRIX T = XMMatrixTranslation(cw.x, cw.y, cw.z);
            return S * R * T;
        }

        bool updateDerived() override { return true; }

        Aabb aabb() const override {
            // Use centerW and axesW with halfExtentsW to compute world-space AABB
            XMFLOAT3 center = centerWorld();
            XMFLOAT3 heW = halfExtentsWorld();
            XMFLOAT3 axes[3];
            axesWorld(axes);
            // extents along world axes
            XMFLOAT3 e{};
            e.x = std::fabs(axes[0].x) * heW.x + std::fabs(axes[1].x) * heW.y + std::fabs(axes[2].x) * heW.z;
            e.y = std::fabs(axes[0].y) * heW.x + std::fabs(axes[1].y) * heW.y + std::fabs(axes[2].y) * heW.z;
            e.z = std::fabs(axes[0].z) * heW.x + std::fabs(axes[1].z) * heW.y + std::fabs(axes[2].z) * heW.z;
            return {
                XMFLOAT3{center.x - e.x, center.y - e.y, center.z - e.z},
                XMFLOAT3{center.x + e.x, center.y + e.y, center.z + e.z}
            };
        }

        void setDebugEnabled(bool enabled) override { m_dbgEnabled = enabled; }
        bool debugEnabled() const override { return m_dbgEnabled; }
        void setDebugColor(const XMFLOAT4 &rgba) override { m_dbgColor = rgba; }
        XMFLOAT4 debugColor() const override { return m_dbgColor; }

        bool setOwnerOffset(const XMFLOAT3 &offset) override {
            m_ownerOffset = offset;
            return true;
        }

        XMFLOAT3 ownerOffset() const override { return m_ownerOffset; }

        void setIsTrigger(bool trigger) override { m_isTrigger = trigger; }
        bool isTrigger() const override { return m_isTrigger; }

        void setIsStatic(bool isStatic) override { m_isStatic = isStatic; }
        bool isStatic() const override { return m_isStatic; }

        // Owner 世界位姿注入/读取
        void setOwnerWorldPosition(const XMFLOAT3 &ownerPosW) override { m_ownerPos = ownerPosW; }
        void setOwnerWorldRotationEuler(const XMFLOAT3 &ownerRotEulerW) override { m_ownerRot = ownerRotEulerW; }
        XMFLOAT3 ownerWorldPosition() const override { return m_ownerPos; }
        XMFLOAT3 ownerWorldRotationEuler() const override { return m_ownerRot; }

        // OBB specifics
        XMFLOAT3 centerWorld() const override {
            XMMATRIX Rowner = XMMatrixRotationRollPitchYaw(m_ownerRot.x, m_ownerRot.y, m_ownerRot.z);
            XMVECTOR off = XMVectorSet(m_localOffset.x, m_localOffset.y, m_localOffset.z, 0);
            off = XMVector3TransformNormal(off, Rowner);
            XMVECTOR base = XMVectorSet(m_ownerPos.x, m_ownerPos.y, m_ownerPos.z, 1.0f);
            XMVECTOR c = XMVectorAdd(base, off);
            XMFLOAT3 cw{};
            XMStoreFloat3(&cw, c);
            return cw;
        }

        void axesWorld(XMFLOAT3 outAxes[3]) const override {
            XMMATRIX Rowner = XMMatrixRotationRollPitchYaw(m_ownerRot.x, m_ownerRot.y, m_ownerRot.z);
            XMMATRIX Rlocal = XMMatrixRotationRollPitchYaw(m_localRot.x, m_localRot.y, m_localRot.z);
            XMMATRIX R = Rlocal * Rowner;
            // Transform unit basis to get world-space orientation (ignoring scale and translation)
            XMVECTOR x = XMVector3TransformNormal(XMVectorSet(1, 0, 0, 0), R);
            XMVECTOR y = XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), R);
            XMVECTOR z = XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), R);
            XMStoreFloat3(&outAxes[0], XMVector3Normalize(x));
            XMStoreFloat3(&outAxes[1], XMVector3Normalize(y));
            XMStoreFloat3(&outAxes[2], XMVector3Normalize(z));
        }

        XMFLOAT3 halfExtentsWorld() const override {
            return XMFLOAT3{
                std::fabs(m_scl.x) * m_halfLocal.x,
                std::fabs(m_scl.y) * m_halfLocal.y,
                std::fabs(m_scl.z) * m_halfLocal.z
            };
        }

        // 射线相交检测（OBB）- 使用 Slab 方法
        bool intersectsRay(const XMFLOAT3 &rayOrigin, const XMFLOAT3 &rayDir, float &outDistance) const override {
            XMFLOAT3 center = centerWorld();
            XMFLOAT3 axes[3];
            axesWorld(axes);
            XMFLOAT3 halfExtents = halfExtentsWorld();

            // 计算从射线起点到 OBB 中心的向量
            XMVECTOR rayOriginVec = XMLoadFloat3(&rayOrigin);
            XMVECTOR centerVec = XMLoadFloat3(&center);
            XMVECTOR delta = XMVectorSubtract(centerVec, rayOriginVec); // 注意：从起点指向中心

            float tMin = 0.0f;  // 射线起点在 t=0
            float tMax = FLT_MAX;

            for (int i = 0; i < 3; ++i) {
                XMVECTOR axis = XMLoadFloat3(&axes[i]);
                float e = XMVectorGetX(XMVector3Dot(axis, delta));      // 中心在轴上的投影
                float f = XMVectorGetX(XMVector3Dot(axis, XMLoadFloat3(&rayDir))); // 方向在轴上的投影

                float extent = (i == 0) ? halfExtents.x : (i == 1) ? halfExtents.y : halfExtents.z;

                if (fabsf(f) > 1e-6f) {
                    // 计算射线与 slab 的交点参数
                    float t1 = (e - extent) / f;
                    float t2 = (e + extent) / f;
                    if (t1 > t2) std::swap(t1, t2);

                    tMin = std::max(tMin, t1);
                    tMax = std::min(tMax, t2);

                    if (tMin > tMax) return false; // 没有交集
                } else {
                    // 射线平行于 slab
                    if (e - extent > 0 || e + extent < 0) return false;
                }
            }

            if (tMax < 0 || tMin > tMax) return false;
            outDistance = tMin;
            return true;
        }

        // 获取世界位置（Owner世界位置 + 旋转后的局部偏移）
        XMFLOAT3 getWorldPosition() const override {
            return centerWorld();  // OBB的世界位置就是中心点
        }

        // 获取世界旋转（Owner世界旋转 + 局部旋转）
        XMFLOAT3 getWorldRotationEuler() const override {
            // 组合旋转：局部旋转 * Owner旋转
            // 对于欧拉角，简单相加（近似，严格应该用四元数）
            return XMFLOAT3{
                m_ownerRot.x + m_localRot.x,
                m_ownerRot.y + m_localRot.y,
                m_ownerRot.z + m_localRot.z
            };
        }

    private:
        XMFLOAT3 m_ownerPos{0, 0, 0};
        XMFLOAT3 m_ownerRot{0, 0, 0};
        XMFLOAT3 m_localOffset{0, 0, 0};
        XMFLOAT3 m_localRot{0, 0, 0};
        XMFLOAT3 m_scl{1, 1, 1};
        XMFLOAT3 m_halfLocal{0.5f, 0.5f, 0.5f};
        bool m_dbgEnabled{false};
        XMFLOAT4 m_dbgColor{0, 1, 0, 1};
        XMFLOAT3 m_ownerOffset{0, 0, 0};
        bool m_isTrigger{false};
        bool m_isStatic{false};
    };

    class CapsuleColliderImpl final : public CapsuleCollider {
    public:
        CapsuleColliderImpl(const XMFLOAT3 &p0Local, const XMFLOAT3 &p1Local, float radiusLocal)
            : m_p0Local(p0Local), m_p1Local(p1Local), m_radiusLocal(std::max(0.0f, radiusLocal)) {
        }

        ColliderType kind() const override { return ColliderType::Capsule; }

        bool setPosition(const XMFLOAT3 &pos) override {
            m_localOffset = pos;
            return true;
        }

        bool setRotationEuler(const XMFLOAT3 &rotEuler) override {
            m_localRot = rotEuler;
            return true;
        }

        bool setScale(const XMFLOAT3 &scale) override {
            if (scale.x <= 0 || scale.y <= 0 || scale.z <= 0) return false;
            // 约束：仅允许沿长轴自由缩放，半径方向必须等比
            const float eps = GetPhysicsConfig().epsilon;
            XMFLOAT3 axis = localAxisUnit();
            // 与坐标轴对齐的阈值：|dot| 接近 1 视为对齐
            float ax = std::fabs(axis.x), ay = std::fabs(axis.y), az = std::fabs(axis.z);
            if (ax >= ay && ax >= az) {
                // 认为与X轴对齐：要求 y==z
                if (!NearlyEqual(scale.y, scale.z, eps)) return false;
            } else if (ay >= ax && ay >= az) {
                // 与Y轴对齐：要求 x==z
                if (!NearlyEqual(scale.x, scale.z, eps)) return false;
            } else {
                // 与Z轴对齐：要求 x==y
                if (!NearlyEqual(scale.x, scale.y, eps)) return false;
            }
            m_scl = scale;
            return true;
        }

        XMFLOAT3 position() const override { return m_localOffset; }
        XMFLOAT3 rotationEuler() const override { return m_localRot; }
        XMFLOAT3 scale() const override { return m_scl; }

        XMMATRIX world() const override {
            XMMATRIX S = XMMatrixScaling(m_scl.x, m_scl.y, m_scl.z);
            XMMATRIX Rowner = XMMatrixRotationRollPitchYaw(m_ownerRot.x, m_ownerRot.y, m_ownerRot.z);
            XMMATRIX Rlocal = XMMatrixRotationRollPitchYaw(m_localRot.x, m_localRot.y, m_localRot.z);
            XMMATRIX R = Rlocal * Rowner;
            XMVECTOR off = XMVectorSet(m_localOffset.x, m_localOffset.y, m_localOffset.z, 0);
            off = XMVector3TransformNormal(off, Rowner);
            XMVECTOR base = XMVectorSet(m_ownerPos.x, m_ownerPos.y, m_ownerPos.z, 1.0f);
            XMVECTOR c = XMVectorAdd(base, off);
            XMFLOAT3 cw{};
            XMStoreFloat3(&cw, c);
            XMMATRIX T = XMMatrixTranslation(cw.x, cw.y, cw.z);
            return S * R * T;
        }

        bool updateDerived() override { return true; }

        Aabb aabb() const override {
            auto seg = segmentWorld();
            XMFLOAT3 p0 = seg.first, p1 = seg.second;
            float r = radiusWorld();
            XMFLOAT3 mn{std::min(p0.x, p1.x) - r, std::min(p0.y, p1.y) - r, std::min(p0.z, p1.z) - r};
            XMFLOAT3 mx{std::max(p0.x, p1.x) + r, std::max(p0.y, p1.y) + r, std::max(p0.z, p1.z) + r};
            return {mn, mx};
        }

        void setDebugEnabled(bool enabled) override { m_dbgEnabled = enabled; }
        bool debugEnabled() const override { return m_dbgEnabled; }
        void setDebugColor(const XMFLOAT4 &rgba) override { m_dbgColor = rgba; }
        XMFLOAT4 debugColor() const override { return m_dbgColor; }

        bool setOwnerOffset(const XMFLOAT3 &offset) override {
            m_ownerOffset = offset;
            return true;
        }

        XMFLOAT3 ownerOffset() const override { return m_ownerOffset; }

        void setIsTrigger(bool trigger) override { m_isTrigger = trigger; }
        bool isTrigger() const override { return m_isTrigger; }

        void setIsStatic(bool isStatic) override { m_isStatic = isStatic; }
        bool isStatic() const override { return m_isStatic; }

        // Owner 世界位姿注入/读取
        void setOwnerWorldPosition(const XMFLOAT3 &ownerPosW) override { m_ownerPos = ownerPosW; }
        void setOwnerWorldRotationEuler(const XMFLOAT3 &ownerRotEulerW) override { m_ownerRot = ownerRotEulerW; }
        XMFLOAT3 ownerWorldPosition() const override { return m_ownerPos; }
        XMFLOAT3 ownerWorldRotationEuler() const override { return m_ownerRot; }

        // 射线相交检测（Capsule）- 简化版：先检测与线段端点球体相交
        bool intersectsRay(const XMFLOAT3 &rayOrigin, const XMFLOAT3 &rayDir, float &outDistance) const override {
            auto seg = segmentWorld();
            XMFLOAT3 p0 = seg.first, p1 = seg.second;
            float radius = radiusWorld();

            XMVECTOR ro = XMLoadFloat3(&rayOrigin);
            XMVECTOR rd = XMLoadFloat3(&rayDir);
            XMVECTOR a = XMLoadFloat3(&p0);
            XMVECTOR b = XMLoadFloat3(&p1);
            XMVECTOR ab = XMVectorSubtract(b, a);
            XMVECTOR ao = XMVectorSubtract(ro, a);

            float ab_ab = XMVectorGetX(XMVector3Dot(ab, ab));
            float ab_ao = XMVectorGetX(XMVector3Dot(ab, ao));
            float ab_rd = XMVectorGetX(XMVector3Dot(ab, rd));

            float m = ab_ab;
            float n = 2.0f * (ab_ao - ab_rd);
            float p = XMVectorGetX(XMVector3Dot(ao, ao)) - 2.0f * ab_ao - radius * radius;

            float discriminant = n * n - 4 * m * p;
            if (discriminant < 0 && ab_ab > 1e-6f) return false;

            // 简化：检测两个端点球体
            auto testSphere = [&](const XMFLOAT3& center) -> float {
                XMVECTOR oc = XMVectorSubtract(ro, XMLoadFloat3(&center));
                float a = XMVectorGetX(XMVector3Dot(rd, rd));
                float b = 2.0f * XMVectorGetX(XMVector3Dot(oc, rd));
                float c = XMVectorGetX(XMVector3Dot(oc, oc)) - radius * radius;
                float disc = b * b - 4 * a * c;
                if (disc < 0) return FLT_MAX;
                float t = (-b - sqrtf(disc)) / (2.0f * a);
                if (t < 0) t = (-b + sqrtf(disc)) / (2.0f * a);
                return (t >= 0) ? t : FLT_MAX;
            };

            float t0 = testSphere(p0);
            float t1 = testSphere(p1);
            float tMin = std::min(t0, t1);

            if (tMin == FLT_MAX) return false;
            outDistance = tMin;
            return true;
        }

        // 获取世界位置（Owner世界位置 + 旋转后的局部偏移）
        XMFLOAT3 getWorldPosition() const override {
            // Capsule的世界位置是线段中点
            auto seg = segmentWorld();
            XMFLOAT3 p0 = seg.first, p1 = seg.second;
            return XMFLOAT3{
                (p0.x + p1.x) * 0.5f,
                (p0.y + p1.y) * 0.5f,
                (p0.z + p1.z) * 0.5f
            };
        }

        // 获取世界旋转（Owner世界旋转 + 局部旋转）
        XMFLOAT3 getWorldRotationEuler() const override {
            // 组合旋转：局部旋转 * Owner旋转
            // 对于欧拉角，简单相加（近似，严格应该用四元数）
            return XMFLOAT3{
                m_ownerRot.x + m_localRot.x,
                m_ownerRot.y + m_localRot.y,
                m_ownerRot.z + m_localRot.z
            };
        }

        // Capsule specifics
        std::pair<XMFLOAT3, XMFLOAT3> segmentWorld() const override {
            XMMATRIX S = XMMatrixScaling(m_scl.x, m_scl.y, m_scl.z);
            XMMATRIX Rowner = XMMatrixRotationRollPitchYaw(m_ownerRot.x, m_ownerRot.y, m_ownerRot.z);
            XMMATRIX Rlocal = XMMatrixRotationRollPitchYaw(m_localRot.x, m_localRot.y, m_localRot.z);
            XMMATRIX R = Rlocal * Rowner;
            XMVECTOR off = XMVectorSet(m_localOffset.x, m_localOffset.y, m_localOffset.z, 0);
            off = XMVector3TransformNormal(off, Rowner);
            XMVECTOR base = XMVectorSet(m_ownerPos.x, m_ownerPos.y, m_ownerPos.z, 1.0f);
            XMVECTOR c = XMVectorAdd(base, off);
            XMFLOAT3 cw{};
            XMStoreFloat3(&cw, c);
            XMMATRIX T = XMMatrixTranslation(cw.x, cw.y, cw.z);
            XMMATRIX M = S * R * T;
            XMVECTOR p0 = XMVectorSet(m_p0Local.x, m_p0Local.y, m_p0Local.z, 1.0f);
            XMVECTOR p1 = XMVectorSet(m_p1Local.x, m_p1Local.y, m_p1Local.z, 1.0f);
            p0 = XMVector3TransformCoord(p0, M);
            p1 = XMVector3TransformCoord(p1, M);
            XMFLOAT3 o0{}, o1{};
            XMStoreFloat3(&o0, p0);
            XMStoreFloat3(&o1, p1);
            return {o0, o1};
        }

        float radiusWorld() const override {
            // 选择半径方向缩放（与长轴正交的等比尺度）
            XMFLOAT3 axis = localAxisUnit();
            float ax = std::fabs(axis.x), ay = std::fabs(axis.y), az = std::fabs(axis.z);
            float rScale = m_scl.x; // default
            if (ax >= ay && ax >= az) {
                // long axis ~ X, radial uses Y/Z (相等已在 setScale 校验)
                rScale = m_scl.y; // == m_scl.z
            } else if (ay >= ax && ay >= az) {
                rScale = m_scl.x; // == m_scl.z
            } else {
                rScale = m_scl.x; // == m_scl.y
            }
            return m_radiusLocal * rScale;
        }

    private:
        XMFLOAT3 localAxisUnit() const {
            XMVECTOR p0 = XMVectorSet(m_p0Local.x, m_p0Local.y, m_p0Local.z, 0);
            XMVECTOR p1 = XMVectorSet(m_p1Local.x, m_p1Local.y, m_p1Local.z, 0);
            XMVECTOR d = XMVectorSubtract(p1, p0);
            d = XMVector3Normalize(d);
            XMFLOAT3 u{};
            XMStoreFloat3(&u, d);
            return u;
        }

        XMFLOAT3 m_ownerPos{0, 0, 0};
        XMFLOAT3 m_ownerRot{0, 0, 0};
        XMFLOAT3 m_localOffset{0, 0, 0};
        XMFLOAT3 m_localRot{0, 0, 0};
        XMFLOAT3 m_scl{1, 1, 1};
        XMFLOAT3 m_p0Local{0, -0.5f, 0};
        XMFLOAT3 m_p1Local{0, 0.5f, 0};
        float m_radiusLocal{0.5f};
        bool m_dbgEnabled{false};
        XMFLOAT4 m_dbgColor{1, 0.5f, 0, 1};
        XMFLOAT3 m_ownerOffset{0, 0, 0};
        bool m_isTrigger{false};
        bool m_isStatic{false};
    };
} // namespace

// ---- 工厂函数 ----
std::unique_ptr<SphereCollider> MakeSphereCollider(float radiusLocal) {
    return std::make_unique<SphereColliderImpl>(radiusLocal);
}

std::unique_ptr<ObbCollider> MakeObbCollider(const XMFLOAT3 &halfExtentsLocal) {
    return std::make_unique<ObbColliderImpl>(halfExtentsLocal);
}

std::unique_ptr<CapsuleCollider> MakeCapsuleCollider(const XMFLOAT3 &p0Local, const XMFLOAT3 &p1Local,
                                                     float radiusLocal) {
    return std::make_unique<CapsuleColliderImpl>(p0Local, p1Local, radiusLocal);
}

std::unique_ptr<CapsuleCollider> MakeCapsuleCollider(float radiusLocal, float cylinderHeight,
                                                     const XMFLOAT3 &axis) {
    // 归一化轴向
    XMVECTOR axisVec = XMLoadFloat3(&axis);
    axisVec = XMVector3Normalize(axisVec);
    XMFLOAT3 normalizedAxis;
    XMStoreFloat3(&normalizedAxis, axisVec);

    // 计算半高度（圆柱体高度的一半）
    float halfHeight = cylinderHeight * 0.5f;

    // 计算两个端点：从原点沿轴向±halfHeight
    XMFLOAT3 p0Local{
        -normalizedAxis.x * halfHeight,
        -normalizedAxis.y * halfHeight,
        -normalizedAxis.z * halfHeight
    };

    XMFLOAT3 p1Local{
        normalizedAxis.x * halfHeight,
        normalizedAxis.y * halfHeight,
        normalizedAxis.z * halfHeight
    };

    return std::make_unique<CapsuleColliderImpl>(p0Local, p1Local, radiusLocal);
}