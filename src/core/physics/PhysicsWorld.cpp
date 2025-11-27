#include "PhysicsWorld.hpp"
#include <algorithm>
#include <cmath>
#include <tuple>

using namespace DirectX;

static inline XMFLOAT3 add3(const XMFLOAT3 &a, const XMFLOAT3 &b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static inline XMFLOAT3 sub3(const XMFLOAT3 &a, const XMFLOAT3 &b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static inline XMFLOAT3 mul3(const XMFLOAT3 &a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static inline float dot3(const XMFLOAT3 &a, const XMFLOAT3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline float len3(const XMFLOAT3 &a) { return std::sqrt(std::max(0.0f, dot3(a, a))); }

void PhysicsWorld::setParams(const WorldParams &p) { params_ = p; }

void PhysicsWorld::registerEntity(EntityId e, RigidBody *rb, std::span<ColliderBase *> cols) {
    if (!rb) return;
    // 若尚未注册 Body，则新建镜像并建立映射
    int idx = rb->bodyIdx;
    if (idx < 0) {
        idx = static_cast<int>(bodies_.size());
        BodyState bs{};
        bs.p = rb->position;
        bs.v = rb->velocity;
        bs.invMass = rb->invMass;
        bs.restitution = rb->restitution;
        bs.muS = rb->muS;
        bs.muK = rb->muK;
        bodies_.push_back(bs);
        bodyRefs_.push_back(rb);
        collidersByBody_.emplace_back();
        rb->bodyIdx = idx;
    }
    entity2bodyIdx_[e] = idx;

    // 绑定 Colliders
    auto &list = entity2colliders_[e];
    for (auto *c: cols) {
        if (!c) continue;
        colliders_.push_back(c);
        col2bodyIdx_[c] = idx;
        col2entity_[c] = e;
        list.push_back(c);
        collidersByBody_[idx].push_back(c);
    }
}

void PhysicsWorld::unregisterEntity(EntityId e) {
    auto itE = entity2colliders_.find(e);
    if (itE == entity2colliders_.end()) return;

    // 移除该实体的 colliders
    auto &cols = itE->second;
    for (auto *c: cols) {
        // 从 colliders_ 移除
        colliders_.erase(std::remove(colliders_.begin(), colliders_.end(), c), colliders_.end());
        // 从 body列表 移除
        auto itB = col2bodyIdx_.find(c);
        if (itB != col2bodyIdx_.end()) {
            int bidx = itB->second;
            auto &byBody = collidersByBody_[bidx];
            byBody.erase(std::remove(byBody.begin(), byBody.end(), c), byBody.end());
        }
        // 清理映射
        col2bodyIdx_.erase(c);
        col2entity_.erase(c);
    }
    cols.clear();
    entity2colliders_.erase(itE);

    // 如果该实体对应的 body 不再被任何 collider 引用，则删除该 body（保持稠密数组）
    auto itBI = entity2bodyIdx_.find(e);
    if (itBI != entity2bodyIdx_.end()) {
        int bidx = itBI->second;
        entity2bodyIdx_.erase(itBI);

        bool referenced = false;
        for (auto &kv: col2bodyIdx_) {
            if (kv.second == bidx) {
                referenced = true;
                break;
            }
        }
        if (!referenced && bidx >= 0 && bidx < static_cast<int>(bodies_.size())) {
            // 交换到末尾并弹出
            int last = static_cast<int>(bodies_.size()) - 1;
            if (bidx != last) {
                std::swap(bodies_[bidx], bodies_[last]);
                std::swap(bodyRefs_[bidx], bodyRefs_[last]);
                std::swap(collidersByBody_[bidx], collidersByBody_[last]);

                // 更新交换后体的 bodyIdx 与所有映射到 last 的 collider 的体索引
                RigidBody *swappedRb = bodyRefs_[bidx];
                if (swappedRb) swappedRb->bodyIdx = bidx;
                for (auto *c: collidersByBody_[bidx]) {
                    col2bodyIdx_[c] = bidx;
                }
                // 同时需要更新与该体绑定的实体的 entity2bodyIdx_
                if (!collidersByBody_[bidx].empty()) {
                    auto *c0 = collidersByBody_[bidx][0];
                    auto itEnt = col2entity_.find(c0);
                    if (itEnt != col2entity_.end()) entity2bodyIdx_[itEnt->second] = bidx;
                }
            }

            bodies_.pop_back();
            bodyRefs_.pop_back();
            collidersByBody_.pop_back();
        }
    }
}

uint64_t PhysicsWorld::PairKey(EntityId a, EntityId b) {
    if (a > b) std::swap(a, b);
    return (static_cast<uint64_t>(a) << 32) | static_cast<uint64_t>(b);
}

void PhysicsWorld::step(float dt) {
    if (dt <= 0) return;
    int sub = std::max(1, params_.substeps);
    float h = dt / static_cast<float>(sub);

    for (int s = 0; s < sub; ++s) {
        integrate(h);
        // 将积分后的 BodyState 位置同步到 Collider，确保 AABB 反映本子步位置
        syncBodiesToColliders();
        broadPhase();
        narrowPhase();
        solveContacts();
        positionalCorrection();
    }
    syncBackAndDispatch(dt);
}

void PhysicsWorld::integrate(float dt) {
    for (size_t i = 0; i < bodies_.size(); ++i) {
        auto &b = bodies_[i];
        if (b.invMass <= 0.0f) continue; // 静态
        // v += g*dt
        b.v = add3(b.v, mul3(params_.gravity, dt));
        // clamp speed
        float L = len3(b.v);
        if (L > params_.maxSpeed) {
            float s = params_.maxSpeed / L;
            b.v = mul3(b.v, s);
        }
        // p += v*dt
        b.p = add3(b.p, mul3(b.v, dt));
    }
}

// 在进行广相/窄相前，将 BodyState 的位置写回到对应的 Collider，刷新 AABB
void PhysicsWorld::syncBodiesToColliders() {
    // 遍历每个刚体，将镜像位置 p 写回其绑定的所有 Collider
    const size_t count = bodies_.size();
    for (size_t i = 0; i < count; ++i) {
        const XMFLOAT3 p = bodies_[i].p;
        if (i >= collidersByBody_.size()) continue;
        auto &cols = collidersByBody_[i];
        for (auto *c: cols) {
            if (!c) continue;
            c->setPosition(p);
            c->updateDerived();
        }
    }
}

void PhysicsWorld::broadPhase() {
    pairs_.clear();

    const size_t n = colliders_.size();
    if (n == 0) return;

    // 小规模时直接使用 O(n^2) 便于省去常数开销
    if (n <= 8) {
        overlapAll(colliders_, pairs_);
        return;
    }

    struct BoxItem {
        ColliderBase *c;
        float minX, maxX, minY, maxY, minZ, maxZ;
    };

    std::vector<BoxItem> items;
    items.reserve(n);
    for (auto *c: colliders_) {
        if (!c) continue;
        Aabb b = c->aabb();
        // 规范化（确保 min <= max）
        float mnx = std::min(b.min.x, b.max.x);
        float mny = std::min(b.min.y, b.max.y);
        float mnz = std::min(b.min.z, b.max.z);
        float mxx = std::max(b.min.x, b.max.x);
        float mxy = std::max(b.min.y, b.max.y);
        float mxz = std::max(b.min.z, b.max.z);
        items.push_back(BoxItem{c, mnx, mxx, mny, mxy, mnz, mxz});
    }

    // 按 minX 排序（Sweep & Prune 单轴）
    std::sort(items.begin(), items.end(), [](const BoxItem &a, const BoxItem &b) {
        if (a.minX == b.minX) return a.maxX < b.maxX;
        return a.minX < b.minX;
    });

    // 活动集合：按 maxX 升序维护可快速剔除
    struct ActiveItem {
        BoxItem box;
    };
    std::vector<ActiveItem> active;
    active.reserve(items.size());

    auto overlap1D = [](float aMin, float aMax, float bMin, float bMax) -> bool {
        return !(aMax < bMin || bMax < aMin);
    };

    for (const auto &it: items) {
        // 移除所有 maxX < 当前 minX 的活动项
        const float curMinX = it.minX;
        size_t write = 0;
        for (size_t k = 0; k < active.size(); ++k) {
            if (active[k].box.maxX >= curMinX) {
                if (write != k) active[write] = active[k];
                ++write;
            }
        }
        active.resize(write);

        // 与仍在活动集中的每个候选做 Y/Z 轴额外 AABB 重叠测试
        for (const auto &aj: active) {
            const BoxItem &bj = aj.box;
            // X 已有重叠（因为在活动集合里），再测 Y/Z
            if (overlap1D(it.minY, it.maxY, bj.minY, bj.maxY) &&
                overlap1D(it.minZ, it.maxZ, bj.minZ, bj.maxZ)) {
                pairs_.emplace_back(bj.c, it.c);
            }
        }

        // 将当前盒加入活动集
        active.push_back(ActiveItem{it});
    }
}

void PhysicsWorld::narrowPhase() {
    contacts_.clear();
    currTriggers_.clear();
    triggerContactMap_.clear();

    for (auto &pr: pairs_) {
        ColliderBase *ca = pr.first;
        ColliderBase *cb = pr.second;
        if (!ca || !cb) continue;
        OverlapResult out{};
        if (!Intersect(*ca, *cb, out) || !out.intersects) continue;

        bool triggerPair = (ca->isTrigger() || cb->isTrigger());
        if (triggerPair) {
            EntityId ea = col2entity_[ca];
            EntityId eb = col2entity_[cb];
            uint64_t key = PairKey(ea, eb);
            currTriggers_.insert(key);
            // 记录一个代表性触点用于回调展示
            triggerContactMap_[key] = out;
            continue;
        }

        int ia = col2bodyIdx_[ca];
        int ib = col2bodyIdx_[cb];
        ContactItem item{};
        item.ia = ia;
        item.ib = ib;
        item.c = out;
        item.ca = ca;
        item.cb = cb;

        // 统一法线方向为 A->B
        XMFLOAT3 ab = sub3(out.pointOnB, out.pointOnA);
        float d = dot3(out.normal, ab);
        if (d < 0) {
            item.c.normal = mul3(out.normal, -1.0f);
        }
        contacts_.push_back(item);
    }
}

void PhysicsWorld::solveContacts() {
    solver_.solve(contacts_, bodies_, params_.solver);
}

void PhysicsWorld::positionalCorrection() {
    // 当前在 ContactSolver::ResolveOne 中已做比例分离，这里留空
}

void PhysicsWorld::syncBackAndDispatch(float /*dt*/) {
    // 写回刚体与碰撞体
    for (size_t i = 0; i < bodies_.size(); ++i) {
        RigidBody *rb = bodyRefs_[i];
        if (!rb) continue;
        const auto &bs = bodies_[i];
        rb->position = bs.p;
        rb->velocity = bs.v;
        // 同步给其 colliders
        for (auto *c: collidersByBody_[i]) {
            if (!c) continue;
            c->setPosition(bs.p);
            c->updateDerived();
        }
    }

    // 触发器事件分发
    if (onTrigger_) {
        // Enter
        for (auto key: currTriggers_) {
            if (lastTriggers_.find(key) == lastTriggers_.end()) {
                // 解析 key 得到实体 id（这里只用于回调顺序一致性，简单处理）
                EntityId a = static_cast<EntityId>(key >> 32);
                EntityId b = static_cast<EntityId>(key & 0xffffffffu);
                onTrigger_(a, b, TriggerPhase::Enter, triggerContactMap_[key]);
            }
        }
        // Stay
        for (auto key: currTriggers_) {
            if (lastTriggers_.find(key) != lastTriggers_.end()) {
                EntityId a = static_cast<EntityId>(key >> 32);
                EntityId b = static_cast<EntityId>(key & 0xffffffffu);
                onTrigger_(a, b, TriggerPhase::Stay, triggerContactMap_[key]);
            }
        }
        // Exit
        for (auto key: lastTriggers_) {
            if (currTriggers_.find(key) == currTriggers_.end()) {
                EntityId a = static_cast<EntityId>(key >> 32);
                EntityId b = static_cast<EntityId>(key & 0xffffffffu);
                OverlapResult dummy{}; // 退出一般无需触点
                onTrigger_(a, b, TriggerPhase::Exit, dummy);
            }
        }
    }

    lastTriggers_ = currTriggers_;
    currTriggers_.clear();
    triggerContactMap_.clear();
    contacts_.clear();
    pairs_.clear();
}
