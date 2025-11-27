#include "ContactSolver.hpp"
#include <algorithm>
#include <cmath>

using namespace DirectX;

// --- 小工具 ---
static inline XMFLOAT3 add3(const XMFLOAT3 &a, const XMFLOAT3 &b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline XMFLOAT3 sub3(const XMFLOAT3 &a, const XMFLOAT3 &b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

static inline XMFLOAT3 mul3(const XMFLOAT3 &a, float s) {
    return {a.x * s, a.y * s, a.z * s};
}

static inline float dot3(const XMFLOAT3 &a, const XMFLOAT3 &b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline float len3(const XMFLOAT3 &a) {
    return std::sqrt(std::max(0.0f, dot3(a, a)));
}

static inline XMFLOAT3 norm3(const XMFLOAT3 &a) {
    float L = len3(a);
    if (L <= 1e-6f) return {0, 1, 0};
    return mul3(a, 1.0f / L);
}

static void ResolveOne(BodyState &A, BodyState &B, const OverlapResult &C, const SolverParams &P) {
    // 统一法线朝向：A -> B
    XMFLOAT3 n = C.normal;
    XMFLOAT3 ab = sub3(C.pointOnB, C.pointOnA);
    if (dot3(n, ab) < 0) n = mul3(n, -1.0f);
    n = norm3(n);

    float invA = A.invMass, invB = B.invMass;
    if (invA + invB == 0.0f) return;

    XMFLOAT3 vRel = sub3(B.v, A.v);
    float vn = dot3(vRel, n);
    if (vn < 0.0f) {
        // 法向冲量
        float e = std::min(A.restitution, B.restitution);
        float j = -(1.0f + e) * vn / (invA + invB);
        A.v = sub3(A.v, mul3(n, j * invA));
        B.v = add3(B.v, mul3(n, j * invB));

        // 动摩擦（简化版本）
        vRel = sub3(B.v, A.v);
        XMFLOAT3 vt = sub3(vRel, mul3(n, dot3(vRel, n)));
        float vtLen = len3(vt);
        if (vtLen > 1e-6f) {
            XMFLOAT3 t = mul3(vt, 1.0f / vtLen);
            float jt = -dot3(vRel, t) / (invA + invB);
            float mu = 0.5f * (A.muK + B.muK);
            float jtMax = mu * j;
            if (jt > jtMax) jt = jtMax;
            else if (jt < -jtMax) jt = -jtMax;
            A.v = sub3(A.v, mul3(t, jt * invA));
            B.v = add3(B.v, mul3(t, jt * invB));
        }
    }

    // 位置校正（比例分离）
    float corr = std::max(0.0f, C.penetration - P.slop) * P.beta;
    if (corr > 0.0f) {
        float sumInv = invA + invB;
        if (sumInv > 0.0f) {
            A.p = sub3(A.p, mul3(n, corr * (invA / sumInv)));
            B.p = add3(B.p, mul3(n, corr * (invB / sumInv)));
        }
    }
}

void ContactSolver::solve(std::vector<ContactItem> &contacts,
                          std::vector<BodyState> &bodies,
                          const SolverParams &params) {
    if (contacts.empty()) return;

    // 可选：按穿透深度排序以加快收敛
    std::stable_sort(contacts.begin(), contacts.end(), [](const ContactItem &a, const ContactItem &b) {
        return a.c.penetration > b.c.penetration;
    });

    for (int it = 0; it < params.iterations; ++it) {
        for (auto &ct: contacts) {
            BodyState &A = bodies[ct.ia];
            BodyState &B = bodies[ct.ib];
            ResolveOne(A, B, ct.c, params);
        }
    }
}
