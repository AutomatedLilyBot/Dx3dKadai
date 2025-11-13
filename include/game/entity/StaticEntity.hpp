#pragma once
#include <memory>
#include <utility>
#include <DirectXMath.h>
#include "core/physics/Transform.hpp"
#include "core/physics/Collider.hpp"
#include "core/render/Drawable.hpp"

// Optional behaviour placeholder
struct IBallAffecter { virtual ~IBallAffecter() = default; virtual void applyForces(float /*dt*/) {} };

class StaticEntity : public IDrawable /*, public IBallAffecter */ {
public:
    Transform transform;
    std::unique_ptr<ICollider> collider; // nullptr for now
    const Model* modelRef = nullptr;     // points to model stored elsewhere
    Material materialData;               // simplified single material

    DirectX::XMMATRIX world() const override { return transform.world(); }
    const Model* model() const override { return modelRef; }
    const Material* material() const override { return &materialData; }
};
