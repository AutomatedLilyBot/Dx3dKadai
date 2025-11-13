#pragma once
#include <cstdint>

// Collision interface (placeholder)
enum class ColliderType { None, Aabb, Obb, Sphere, Compound };

struct ICollider {
    virtual ~ICollider() = default;

    virtual ColliderType type() const = 0;
};